#include <unity.h>
#include "GameCore.h"
#include <cstring>

void setUp() {}
void tearDown() {}

gd::Game makeGame(unsigned id, const char *state, unsigned away, unsigned home) {
    gd::Game g;
    g.id = id;
    std::strncpy(g.state, state, sizeof(g.state)-1);
    g.awayScore = away;
    g.homeScore = home;
    return g;
}
void test_goal_increase_live() {
    gd::Game a=makeGame(123,"LIVE",1,1), b=makeGame(123,"LIVE",1,2);
    auto change=gd::detectGoalChange(a,b);
    TEST_ASSERT_TRUE(change.changed);
    TEST_ASSERT_EQUAL_UINT8(0, change.awayAdded);
    TEST_ASSERT_EQUAL_UINT8(1, change.homeAdded);
}
void test_baseline_different_game_not_goal() {
    auto change=gd::detectGoalChange(makeGame(0,"LIVE",0,0),makeGame(123,"LIVE",2,1));
    TEST_ASSERT_FALSE(change.changed);
    change=gd::detectGoalChange(makeGame(222,"LIVE",0,0),makeGame(123,"LIVE",2,1));
    TEST_ASSERT_FALSE(change.changed);
}
void test_score_correction_not_goal() {
    auto change=gd::detectGoalChange(makeGame(123,"LIVE",2,1),makeGame(123,"LIVE",1,1));
    TEST_ASSERT_FALSE(change.changed);
}
void test_non_live_not_goal() {
    TEST_ASSERT_FALSE(gd::detectGoalChange(makeGame(5,"FUT",0,0),makeGame(5,"FINAL",3,2)).changed);
}
void test_live_crit() {
    TEST_ASSERT_TRUE(gd::isLive(makeGame(5,"LIVE",1,2)));
    TEST_ASSERT_TRUE(gd::isLive(makeGame(5,"CRIT",1,2)));
    TEST_ASSERT_FALSE(gd::isLive(makeGame(5,"OFF",1,2)));
}
void test_find_game() {
    gd::Scoreboard s; s.count=2;
    s.games[0]=makeGame(10,"LIVE",1,1);
    s.games[1]=makeGame(20,"FUT",0,0);
    TEST_ASSERT_NOT_NULL(gd::findGame(s,20));
    TEST_ASSERT_NULL(gd::findGame(s,30));
}
void test_colors() {
    TEST_ASSERT_EQUAL_HEX32(0x6CACE4, gd::teamColor("UTA"));
    TEST_ASSERT_EQUAL_HEX32(0xFFB81C, gd::teamColor("BOS"));
    TEST_ASSERT_EQUAL_HEX32(0x31837A, gd::teamColor("BAD"));
}
int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_goal_increase_live);
    RUN_TEST(test_baseline_different_game_not_goal);
    RUN_TEST(test_score_correction_not_goal);
    RUN_TEST(test_non_live_not_goal);
    RUN_TEST(test_live_crit);
    RUN_TEST(test_find_game);
    RUN_TEST(test_colors);
    return UNITY_END();
}
