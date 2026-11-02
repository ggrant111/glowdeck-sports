#include <unity.h>
#include "GoalEngine.h"
#include <cstring>

namespace {
gd::Game game(unsigned id, const char* state, unsigned away, unsigned home) {
    gd::Game g;
    g.id = id;
    std::strncpy(g.away, "UTA", sizeof(g.away)-1);
    std::strncpy(g.home, "BOS", sizeof(g.home)-1);
    std::strncpy(g.state, state, sizeof(g.state)-1);
    g.awayScore = away;
    g.homeScore = home;
    return g;
}
struct FakeLight : gd::LightPort {
    unsigned starts = 0, restores = 0;
    unsigned failures = 0;
    bool outstanding = false;
    bool failRestore = false;
    uint32_t lastColor = 0;
    bool startColor(uint32_t rgb) override {
        if (outstanding) return false;
        outstanding = true; // journal must exist before a color write
        ++starts; lastColor = rgb;
        return true;
    }
    bool restore() override {
        if (!outstanding) return true;
        if (failRestore) { ++failures; return false; }
        outstanding = false;
        ++restores;
        return true;
    }
    bool needsRestore() const override { return outstanding; }
};
}

void test_default_disarmed_and_requires_live_game() {
    gd::GoalEngine e;
    gd::Game g = game(1,"FUT",0,0);
    e.selectGame(1);
    TEST_ASSERT_FALSE(e.armed());
    TEST_ASSERT_FALSE(e.arm(&g,0));
    g=game(1,"LIVE",1,0);
    TEST_ASSERT_TRUE(e.arm(&g,10));
    TEST_ASSERT_TRUE(e.armed());
}
void test_goal_dedup_and_delay() {
    gd::GoalEngine e;
    e.selectGame(1); e.setFavoriteHome(false); e.setDelay(30);
    gd::Game g = game(1,"LIVE",0,0);
    TEST_ASSERT_TRUE(e.arm(&g,1000));
    g=game(1,"LIVE",1,0); e.observe(&g,2000);
    TEST_ASSERT_TRUE(e.pending());
    TEST_ASSERT_FALSE(e.ready(31000));
    e.observe(&g,31000); // duplicate poll
    TEST_ASSERT_TRUE(e.pending());
    TEST_ASSERT_TRUE(e.ready(32000));
    auto goal=e.consume(32000);
    TEST_ASSERT_TRUE(goal.valid);
    TEST_ASSERT_FALSE(goal.home);
    TEST_ASSERT_TRUE(goal.favorite);
    TEST_ASSERT_EQUAL_UINT8(1,goal.score);
    TEST_ASSERT_FALSE(e.consume(32000).valid);
}
void test_correction_and_multigoal_suppress() {
    gd::GoalEngine e; e.selectGame(1);
    gd::Game g=game(1,"LIVE",2,1);
    TEST_ASSERT_TRUE(e.arm(&g,0));
    g=game(1,"LIVE",1,1); e.observe(&g,1000);
    TEST_ASSERT_FALSE(e.pending());
    TEST_ASSERT_FALSE(e.armed()); // correction requires manual re-arm
    g=game(1,"LIVE",3,1); e.observe(&g,2000); // missing polls
    TEST_ASSERT_FALSE(e.pending());
    TEST_ASSERT_TRUE(e.arm(&g,2500));
    g=game(1,"LIVE",3,2); e.observe(&g,3000);
    TEST_ASSERT_TRUE(e.pending());
}
void test_game_switch_and_disarm_cancel() {
    gd::GoalEngine e; e.selectGame(1); e.setDelay(60);
    gd::Game g=game(1,"LIVE",0,0);
    TEST_ASSERT_TRUE(e.arm(&g,0));
    g=game(1,"LIVE",0,1); e.observe(&g,1000);
    TEST_ASSERT_TRUE(e.pending());
    e.selectGame(2); TEST_ASSERT_FALSE(e.armed()); TEST_ASSERT_FALSE(e.pending());
    e.observe(&g,90000); TEST_ASSERT_FALSE(e.ready(90000));
}
void test_network_outage_and_stale_disarm() {
    gd::GoalEngine e; gd::Game g=game(1,"LIVE",0,0);
    e.selectGame(1); e.setDelay(90); e.arm(&g,0);
    g=game(1,"LIVE",0,1); e.observe(&g,1000);
    e.expire(125000); TEST_ASSERT_FALSE(e.armed()); TEST_ASSERT_FALSE(e.pending());
    // Reconnect snapshot with a higher score never triggers without explicit arm.
    g=game(1,"LIVE",0,2); e.observe(&g,126000);
    TEST_ASSERT_FALSE(e.pending());
}
void test_reaction_snapshot_restore_and_cooldown() {
    gd::GoalEngine e; FakeLight light; gd::ReactionController r(e,light);
    e.selectGame(1); e.setFavoriteHome(false); e.setDelay(0);
    gd::Game g=game(1,"LIVE",0,0); e.arm(&g,1000);
    r.setTeamColors(0x6CACE4,0xFFB81C);
    g=game(1,"LIVE",1,0); e.observe(&g,2000);
    r.service(2000,true);
    TEST_ASSERT_EQUAL_UINT32(1,light.starts);
    TEST_ASSERT_EQUAL_HEX32(0x6CACE4,light.lastColor);
    TEST_ASSERT_TRUE(light.outstanding);
    g=game(1,"LIVE",1,1); e.observe(&g,3000);
    TEST_ASSERT_FALSE(e.pending()); // cooldown suppresses overlap
    r.service(11000,true);
    TEST_ASSERT_EQUAL_UINT32(0,light.restores);
    r.service(12000,true);
    TEST_ASSERT_EQUAL_UINT32(1,light.restores);
    TEST_ASSERT_FALSE(light.outstanding);
}
void test_disarm_restores_and_retry_after_wifi() {
    gd::GoalEngine e; FakeLight light; gd::ReactionController r(e,light);
    e.selectGame(1);
    gd::Game g=game(1,"LIVE",0,0); e.arm(&g,0);
    g=game(1,"LIVE",0,1); e.observe(&g,1000);
    r.service(1000,true); TEST_ASSERT_TRUE(light.outstanding);
    light.failRestore=true;
    e.disarm(); r.service(1050,true);
    TEST_ASSERT_TRUE(light.outstanding);
    r.service(1100,false);
    TEST_ASSERT_EQUAL_UINT32(1,light.failures);
    light.failRestore=false;
    r.service(4100,true);
    TEST_ASSERT_FALSE(light.outstanding);
    TEST_ASSERT_EQUAL_UINT32(1,r.restoreCount());
}
void test_reboot_journal_recovery_without_rearming() {
    FakeLight light; light.outstanding=true; // NVS journal persisted across reset
    gd::GoalEngine e; gd::ReactionController restarted(e,light);
    restarted.service(1000,false);
    TEST_ASSERT_TRUE(light.outstanding);
    restarted.service(2000,true);
    TEST_ASSERT_EQUAL_UINT32(1,light.restores);
    TEST_ASSERT_FALSE(e.armed());
}
void test_score_final_and_multi_side_jump_suppress() {
    gd::GoalEngine e; e.selectGame(1);
    gd::Game g=game(1,"LIVE",1,1); e.arm(&g,0);
    g=game(1,"LIVE",2,2); e.observe(&g,1000);
    TEST_ASSERT_FALSE(e.pending());
    TEST_ASSERT_FALSE(e.armed());
    g=game(1,"OFF",2,2); e.observe(&g,2000);
    TEST_ASSERT_FALSE(e.armed());
}
void test_timer_wraparound() {
    gd::GoalEngine e; e.selectGame(1); e.setDelay(15);
    gd::Game g=game(1,"LIVE",1,1);
    uint32_t now=0xFFFFFF00U;
    e.arm(&g,now);
    g=game(1,"LIVE",2,1);
    e.observe(&g,now);
    TEST_ASSERT_FALSE(e.ready(now));
    TEST_ASSERT_TRUE(e.ready(now+15000U));
}
