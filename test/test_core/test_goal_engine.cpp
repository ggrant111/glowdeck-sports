#include <unity.h>
#include "GoalEngine.h"
#include <cstring>
#include <initializer_list>
#include <utility>
namespace {
gd::Game game(unsigned id=1,const char *state="LIVE") {
    gd::Game g; g.id=id;
    std::strncpy(g.away,"UTA",4); std::strncpy(g.home,"BOS",4);
    std::strncpy(g.state,state,sizeof(g.state)-1); return g;
}
gd::GoalFeed feed(uint32_t latest,std::initializer_list<std::pair<uint32_t,uint32_t>> goals={},
                  uint32_t gameId=1,uint32_t away=11,uint32_t home=22) {
    gd::GoalFeed f; f.gameId=gameId; f.awayId=away; f.homeId=home;
    f.latestEventId=latest; f.valid=true;
    for(auto x:goals){f.goals[f.count].id=x.first;f.goals[f.count].ownerId=x.second;++f.count;}
    return f;
}
struct FakeLight : gd::LightPort {
    uint32_t lastColor=0;
    unsigned starts=0,restores=0,failures=0;
    bool outstanding=false,failRestore=false;
    bool startColor(uint32_t color) override {
        if(outstanding)return false;
        ++starts;lastColor=color;outstanding=true;return true;
    }
    bool restore() override {
        if(!outstanding)return true;
        if(failRestore){++failures;return false;}
        ++restores;outstanding=false;return true;
    }
    bool needsRestore() const override {return outstanding;}
};
}
void test_default_disarmed_and_accepts_upcoming_game() {
    gd::GoalEngine e; e.selectGame(1);
    gd::Game g=game(1,"FUT");TEST_ASSERT_TRUE(e.arm(&g,0));TEST_ASSERT_TRUE(e.waiting());
    g=game(1,"OFF");TEST_ASSERT_FALSE(e.arm(&g,0));
    g=game();TEST_ASSERT_TRUE(e.arm(&g,100));
    TEST_ASSERT_TRUE(e.armed());TEST_ASSERT_TRUE(e.paused());
    TEST_ASSERT_FALSE(e.baselined());
}
void test_goal_dedup_and_delay() {
    gd::GoalEngine e;e.selectGame(1);e.setDelay(30);
    gd::Game g=game();e.arm(&g,1000);
    e.observeFeed(feed(10,{{5,11}}),1000);
    TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(20,{{5,11},{20,11}}),2000);
    TEST_ASSERT_TRUE(e.pending());TEST_ASSERT_FALSE(e.ready(31000));
    e.observeFeed(feed(20,{{5,11},{20,11}}),31000);
    TEST_ASSERT_TRUE(e.ready(32000));
    auto x=e.consume(32000);
    TEST_ASSERT_TRUE(x.valid);TEST_ASSERT_FALSE(x.home);
    TEST_ASSERT_EQUAL_UINT32(20,x.eventId);
    TEST_ASSERT_FALSE(e.consume(32000).valid);
}
void test_correction_and_multigoal_suppress() {
    gd::GoalEngine e;e.selectGame(1);gd::Game g=game();e.arm(&g,0);
    e.observeFeed(feed(10),0);
    e.observeFeed(feed(20,{{18,11},{19,22}}),1000);
    TEST_ASSERT_FALSE(e.pending());TEST_ASSERT_TRUE(e.armed());
    e.observeFeed(feed(11),2000); // feed rewound; rebaseline, not trigger
    TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(25,{{24,22}}),3000);
    TEST_ASSERT_FALSE(e.pending()); // default away only
}
void test_game_switch_and_disarm_cancel() {
    gd::GoalEngine e;e.selectGame(1);e.setDelay(60);
    gd::Game g=game();e.arm(&g,0);e.observeFeed(feed(1),0);
    e.observeFeed(feed(2,{{2,11}}),1000);
    TEST_ASSERT_TRUE(e.pending());
    e.selectGame(2);TEST_ASSERT_FALSE(e.armed());TEST_ASSERT_FALSE(e.pending());
}
void test_network_outage_and_stale_disarm() {
    gd::GoalEngine e;e.selectGame(1);e.setDelay(60);
    gd::Game g=game();e.arm(&g,0);e.observeFeed(feed(1),0);
    e.observeFeed(feed(2,{{2,11}}),1000);
    e.expire(77000);TEST_ASSERT_TRUE(e.armed());
    TEST_ASSERT_TRUE(e.paused());TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(10,{{2,11},{10,11}}),78000); // baseline after outage
    TEST_ASSERT_FALSE(e.pending());
    TEST_ASSERT_FALSE(e.paused());
}
void test_reaction_snapshot_restore_and_cooldown() {
    gd::GoalEngine e;FakeLight light;gd::ReactionController r(e,light);
    e.selectGame(1);gd::Game g=game();e.arm(&g,1000);
    r.setTeamColors(0x6CACE4,0xFFB81C);
    e.observeFeed(feed(1),1000);
    e.observeFeed(feed(2,{{2,11}}),2000);
    r.service(2000,true);
    TEST_ASSERT_EQUAL_UINT32(1,light.starts);
    TEST_ASSERT_EQUAL_HEX32(0x6CACE4,light.lastColor);
    e.observeFeed(feed(3,{{2,11},{3,11}}),3000);
    TEST_ASSERT_FALSE(e.pending());
    r.service(9000,true);TEST_ASSERT_EQUAL_UINT32(0,light.restores);
    r.service(10000,true);TEST_ASSERT_EQUAL_UINT32(1,light.restores);
}
void test_disarm_restores_and_retry_after_wifi() {
    gd::GoalEngine e;FakeLight light;gd::ReactionController r(e,light);
    e.selectGame(1);gd::Game g=game();e.arm(&g,0);
    e.observeFeed(feed(1),0);e.observeFeed(feed(2,{{2,11}}),1000);
    r.service(1000,true);TEST_ASSERT_TRUE(light.outstanding);
    light.failRestore=true;e.disarm();r.service(1050,true);
    TEST_ASSERT_TRUE(light.outstanding);
    r.service(1100,false);
    TEST_ASSERT_EQUAL_UINT32(1,light.failures);
    light.failRestore=false;r.service(4100,true);
    TEST_ASSERT_FALSE(light.outstanding);
}
void test_reboot_journal_recovery_without_rearming() {
    gd::GoalEngine e; FakeLight l; l.outstanding=true;
    gd::ReactionController r(e,l);r.service(1000,false);
    r.service(2000,true);TEST_ASSERT_EQUAL_UINT32(1,l.restores);
    TEST_ASSERT_FALSE(e.armed());
}
void test_score_final_and_multi_side_jump_suppress() {
    gd::GoalEngine e;e.selectGame(1);gd::Game g=game();e.arm(&g,0);
    e.observeFeed(feed(1),0);
    e.observeFeed(feed(3,{{2,11},{3,22}}),1000);
    TEST_ASSERT_FALSE(e.pending());
    g=game(1,"OFF");e.observeGame(&g);TEST_ASSERT_FALSE(e.armed());
}
void test_timer_wraparound() {
    gd::GoalEngine e;e.selectGame(1);e.setDelay(15);
    gd::Game g=game();uint32_t now=0xFFFFFF00U;
    e.arm(&g,now);e.observeFeed(feed(1),now);
    e.observeFeed(feed(2,{{2,11}}),now+100);
    TEST_ASSERT_FALSE(e.ready(now+100));
    TEST_ASSERT_TRUE(e.ready(now+15100U));
}
void test_away_home_both_modes() {
    gd::GoalEngine e;gd::Game g=game();e.selectGame(1);
    e.setMode(gd::CelebrationMode::Home);e.arm(&g,0);
    e.observeFeed(feed(1),0);
    e.observeFeed(feed(2,{{2,11}}),1000);TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(3,{{2,11},{3,22}}),2000);
    TEST_ASSERT_TRUE(e.pending());TEST_ASSERT_TRUE(e.consume(2000).home);
    e.disarm();e.setMode(gd::CelebrationMode::Both);e.arm(&g,3000);
    e.observeFeed(feed(3,{{2,11},{3,22}}),3000);
    e.observeFeed(feed(4,{{2,11},{3,22},{4,11}}),4000);
    TEST_ASSERT_TRUE(e.pending());TEST_ASSERT_FALSE(e.consume(4000).home);
}
void test_both_team_uses_scoring_color() {
    gd::GoalEngine e;gd::Game g=game();FakeLight light;gd::ReactionController r(e,light);
    e.selectGame(1);e.setMode(gd::CelebrationMode::Both);e.arm(&g,0);
    r.setTeamColors(0x6CACE4,0xFFB81C);
    e.observeFeed(feed(1),0);e.observeFeed(feed(2,{{2,22}}),1000);
    r.service(1000,true);TEST_ASSERT_EQUAL_HEX32(0xFFB81C,light.lastColor);
    r.service(9000,true);TEST_ASSERT_FALSE(light.outstanding);
    e.observeFeed(feed(3,{{2,22},{3,11}}),22000);
    r.service(22000,true);TEST_ASSERT_EQUAL_HEX32(0x6CACE4,light.lastColor);
}
void test_unknown_goal_owner_never_triggers() {
    gd::GoalEngine e;gd::Game g=game();e.selectGame(1);
    e.setMode(gd::CelebrationMode::Both);e.arm(&g,0);
    e.observeFeed(feed(1),0);
    e.observeFeed(feed(2,{{2,999}}),1000);
    TEST_ASSERT_FALSE(e.pending());
}
void test_pause_resumes_by_baseline_without_replay() {
    gd::GoalEngine e;gd::Game g=game();e.selectGame(1);
    e.setMode(gd::CelebrationMode::Both);e.arm(&g,0);
    e.observeFeed(feed(1),0);
    e.pause();TEST_ASSERT_TRUE(e.paused());
    e.observeFeed(feed(3,{{2,11},{3,22}}),30000);
    TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(4,{{2,11},{3,22},{4,11}}),45000);
    TEST_ASSERT_TRUE(e.pending());
}
void test_invalid_teams_and_invalid_mode() {
    gd::GoalEngine e;gd::Game g=game();e.selectGame(1);
    e.setMode(static_cast<gd::CelebrationMode>(99));
    TEST_ASSERT_TRUE(e.mode()==gd::CelebrationMode::Away);
    e.arm(&g,0);
    e.observeFeed(feed(1,{},1,0,22),0);
    TEST_ASSERT_TRUE(e.paused());
    e.observeFeed(feed(10,{},1,11,22),1000);
    TEST_ASSERT_FALSE(e.paused());
    e.observeFeed(feed(11,{{11,22}}),2000);
    TEST_ASSERT_FALSE(e.pending());
}
void test_multi_goal_jump_and_duplicate_event_ids() {
    gd::GoalEngine e;gd::Game g=game();e.selectGame(1);
    e.setMode(gd::CelebrationMode::Both);e.arm(&g,0);
    e.observeFeed(feed(1),0);
    e.observeFeed(feed(4,{{3,11},{4,22}}),1000);
    TEST_ASSERT_FALSE(e.pending());
    e.observeFeed(feed(4,{{3,11},{4,22}}),2000);
    TEST_ASSERT_FALSE(e.pending());
}

void test_pregame_start_and_goal_baseline() {
    gd::GoalEngine e;e.selectGame(1);e.setEventMask(127,true);gd::Game g=game(1,"FUT");
    TEST_ASSERT_TRUE(e.arm(&g,0));auto pre=feed(0);strcpy(pre.state,"PRE");
    e.observeFeed(pre,1000);TEST_ASSERT_TRUE(e.waiting());TEST_ASSERT_FALSE(e.pending());
    auto live=feed(1);strcpy(live.state,"LIVE");live.extraCount=1;live.extra[0].id=1;live.extra[0].kind=gd::SportEvent::PeriodStart;
    e.observeFeed(live,2000);TEST_ASSERT_FALSE(e.waiting());TEST_ASSERT_TRUE(e.ready(2000));
    TEST_ASSERT_EQUAL_INT(int(gd::SportEvent::PeriodStart),int(e.consume(2000).kind));
    e.observeFeed(feed(2,{{2,11}}),23000);TEST_ASSERT_TRUE(e.ready(23000));
}
void test_pregame_outage_cancel_and_missing_board() {
    gd::GoalEngine e;e.selectGame(1);gd::Game g=game(1,"FUT");e.arm(&g,0);
    auto pre=feed(0);strcpy(pre.state,"FUT");e.observeFeed(pre,0);
    e.observeGame(nullptr);TEST_ASSERT_TRUE(e.armed());TEST_ASSERT_TRUE(e.paused());
    auto live=feed(4,{{4,11}});strcpy(live.state,"LIVE");e.observeFeed(live,10000);
    TEST_ASSERT_FALSE(e.pending());e.observeFeed(feed(5,{{4,11},{5,11}}),20000);TEST_ASSERT_TRUE(e.pending());
    auto cancelled=feed(5);strcpy(cancelled.state,"CNCL");e.observeFeed(cancelled,21000);
    TEST_ASSERT_FALSE(e.armed());TEST_ASSERT_FALSE(e.pending());
}
void test_pregame_never_triggers_and_postponed_waits() {
    gd::GoalEngine e;e.selectGame(1);gd::Game g=game(1,"FUT");e.arm(&g,0);
    auto pre=feed(3,{{3,11}});strcpy(pre.state,"PRE");e.observeFeed(pre,1);
    TEST_ASSERT_FALSE(e.pending());TEST_ASSERT_FALSE(e.ready(1));
    strcpy(pre.state,"PPD");e.observeFeed(pre,2);TEST_ASSERT_TRUE(e.armed());TEST_ASSERT_TRUE(e.waiting());
    e.disarm();TEST_ASSERT_FALSE(e.waiting());
}
