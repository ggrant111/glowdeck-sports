#include <unity.h>
#include "ReactionQueue.h"
#include "GoalEngine.h"
#include <cstring>
using namespace gd;
void test_follow_priority_fifo(){ReactionQueue q;QueuedReaction a,b,out;a.game=0;a.kind=SportEvent::PowerPlay;b.game=1;b.kind=SportEvent::Goal;
 TEST_ASSERT_TRUE(q.push(a,100));TEST_ASSERT_TRUE(q.push(b,101));b.game=2;q.push(b,102);
 TEST_ASSERT_TRUE(q.pop(103,out));TEST_ASSERT_EQUAL(1,out.game);q.pop(103,out);TEST_ASSERT_EQUAL(2,out.game);q.pop(103,out);TEST_ASSERT_EQUAL(0,out.game);TEST_ASSERT_FALSE(q.pop(103,out));}
void test_follow_expiry_cancel_wrap(){ReactionQueue q;QueuedReaction a,out;a.game=1;q.push(a,0xfffffff0);q.expire(100);TEST_ASSERT_EQUAL(1,q.size());q.expire(20000);TEST_ASSERT_EQUAL(0,q.size());q.push(a,1);a.game=2;q.push(a,1);q.removeGame(1);q.pop(10,out);TEST_ASSERT_EQUAL(2,out.game);q.clear();TEST_ASSERT_EQUAL(0,q.size());}
void test_follow_overflow_priority(){ReactionQueue q;QueuedReaction a,out;a.kind=SportEvent::PowerPlay;for(int i=0;i<8;++i)q.push(a,10);TEST_ASSERT_FALSE(q.push(a,10));a.kind=SportEvent::Goal;a.game=3;TEST_ASSERT_TRUE(q.push(a,10));TEST_ASSERT_EQUAL(8,q.size());q.pop(11,out);TEST_ASSERT_EQUAL(3,out.game);}
void test_follow_independent_games(){GoalEngine engines[4];GoalFeed feed;feed.valid=true;feed.awayId=1;feed.homeId=2;strcpy(feed.state,"LIVE");Game g;strcpy(g.state,"LIVE");
 for(int i=0;i<4;i++){g.id=2000000001+i;engines[i].selectGame(g.id);engines[i].setMode(CelebrationMode::Both);TEST_ASSERT_TRUE(engines[i].arm(&g,0));feed.gameId=g.id;feed.latestEventId=10;feed.count=0;engines[i].observeFeed(feed,100);}
 for(int i=0;i<4;i++){feed.gameId=2000000001+i;feed.latestEventId=11;feed.count=1;feed.goals[0].id=11;feed.goals[0].ownerId=1;engines[i].observeFeed(feed,200);TEST_ASSERT_TRUE(engines[i].ready(200));}
 engines[0].pause();TEST_ASSERT_FALSE(engines[0].ready(200));for(int i=1;i<4;i++){TEST_ASSERT_TRUE(engines[i].consume(200).valid);engines[i].observeFeed(feed,300);TEST_ASSERT_FALSE(engines[i].ready(300));}}
