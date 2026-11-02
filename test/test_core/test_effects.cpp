#include <unity.h>
#include "GoalEngine.h"
#include <cstring>
void test_ddp_packet_boundaries(){
 uint8_t b[1450];size_t n=gd::ddpPacket(b,sizeof b,gd::SportEvent::Goal,0x6cace4,600,0,0,15);
 TEST_ASSERT_EQUAL(1450,n);TEST_ASSERT_EQUAL_HEX8(0x40,b[0]);TEST_ASSERT_EQUAL(15,b[1]);TEST_ASSERT_EQUAL(11,b[2]);TEST_ASSERT_EQUAL(1,b[3]);
 TEST_ASSERT_EQUAL(5,b[8]);TEST_ASSERT_EQUAL(160,b[9]);TEST_ASSERT_EQUAL_HEX8(67,b[10]);
 n=gd::ddpPacket(b,sizeof b,gd::SportEvent::Goal,0x6cace4,600,480,0,1);
 TEST_ASSERT_EQUAL(370,n);TEST_ASSERT_EQUAL_HEX8(0x41,b[0]);TEST_ASSERT_EQUAL(5,b[6]);TEST_ASSERT_EQUAL(160,b[7]);
 TEST_ASSERT_EQUAL(0,gd::ddpPacket(b,sizeof b,gd::SportEvent::Goal,0,601,0,0,0));
 TEST_ASSERT_EQUAL(0,gd::ddpPacket(b,9,gd::SportEvent::Goal,0,1,0,0,0));
 TEST_ASSERT_EQUAL(0,gd::ddpPacket(b,sizeof b,gd::SportEvent::Goal,0,1,1,0,0));
}
void test_manpower_and_period_types(){
 TEST_ASSERT_EQUAL(0,gd::advantage("1551"));TEST_ASSERT_EQUAL(1,gd::advantage("1541"));TEST_ASSERT_EQUAL(2,gd::advantage("1451"));
 TEST_ASSERT_EQUAL(-1,gd::advantage("0651"));TEST_ASSERT_EQUAL(-1,gd::advantage("1550"));TEST_ASSERT_EQUAL(-1,gd::advantage(""));
 TEST_ASSERT_EQUAL(int(gd::SportEvent::Overtime),int(gd::periodStart("OT")));
 TEST_ASSERT_EQUAL(int(gd::SportEvent::Shootout),int(gd::periodStart("SO")));
}
static gd::Game live(){gd::Game g;g.id=2026020001;std::strcpy(g.state,"LIVE");return g;}
static gd::GoalFeed feed(){gd::GoalFeed f;f.valid=true;f.gameId=2026020001;f.homeId=2;f.awayId=1;f.latestEventId=10;return f;}
void test_event_baseline_delay_mask_and_dedupe(){
 auto g=live();auto f=feed();gd::GoalEngine e;e.selectGame(g.id);e.setEventMask(127);e.setDelay(15);e.arm(&g,0);
 f.extraCount=1;f.extra[0]={10,0,gd::SportEvent::PeriodStart};e.observeFeed(f,0);TEST_ASSERT_FALSE(e.pending());
 f.latestEventId=11;f.extra[0].id=11;e.observeFeed(f,1000);TEST_ASSERT_FALSE(e.ready(15999));TEST_ASSERT_TRUE(e.ready(16000));
 TEST_ASSERT_EQUAL(int(gd::SportEvent::PeriodStart),int(e.consume(16000).kind));
 e.observeFeed(f,40000);TEST_ASSERT_FALSE(e.pending());
 e.setEventMask(1);f.latestEventId=12;f.extra[0].id=12;e.observeFeed(f,41000);TEST_ASSERT_FALSE(e.pending());
}
void test_goal_then_final_and_outage_baseline(){
 auto g=live();auto f=feed();gd::GoalEngine e;e.selectGame(g.id);e.setEventMask(127);e.arm(&g,0);e.observeFeed(f,0);
 f.latestEventId=12;f.count=1;f.goals[0]={11,1};f.final=true;f.extraCount=1;f.extra[0]={12,0,gd::SportEvent::GameEnd};
 std::strcpy(g.state,"OFF");e.observeGame(&g);TEST_ASSERT_TRUE(e.armed());e.observeFeed(f,1000);
 TEST_ASSERT_EQUAL(int(gd::SportEvent::Goal),int(e.consume(1000).kind));
 e.observeFeed(f,26000);TEST_ASSERT_TRUE(e.ready(26000));TEST_ASSERT_EQUAL(int(gd::SportEvent::GameEnd),int(e.consume(26000).kind));
 e.observeFeed(f,51000);TEST_ASSERT_FALSE(e.pending());f.latestEventId=13;f.extra[0].id=13;e.observeFeed(f,51500);TEST_ASSERT_FALSE(e.pending());e.pause();f.latestEventId=13;f.extra[0].id=13;e.observeFeed(f,52000);TEST_ASSERT_FALSE(e.pending());
}
void test_team_powerplay_filter_and_neutral_events(){
 auto g=live();auto f=feed();gd::GoalEngine e;e.selectGame(g.id);e.setEventMask(127);e.arm(&g,0);e.observeFeed(f,0);
 f.latestEventId=11;f.extraCount=1;f.extra[0]={11,2,gd::SportEvent::PowerPlay};e.observeFeed(f,1000);TEST_ASSERT_FALSE(e.pending());
 f.latestEventId=12;f.extra[0]={12,0,gd::SportEvent::Overtime};e.observeFeed(f,2000);TEST_ASSERT_TRUE(e.ready(2000));
 TEST_ASSERT_EQUAL(int(gd::SportEvent::Overtime),int(e.consume(2000).kind));
}

void test_ddp_goal_only_keeps_final_goal_window(){
 auto g=live();auto f=feed();gd::GoalEngine e;e.selectGame(g.id);e.setEventMask(1,true);e.arm(&g,0);e.observeFeed(f,0);
 std::strcpy(g.state,"OFF");e.observeGame(&g);TEST_ASSERT_TRUE(e.armed());
 f.final=true;f.latestEventId=11;f.count=1;f.goals[0]={11,1};e.observeFeed(f,1000);TEST_ASSERT_TRUE(e.ready(1000));
}
