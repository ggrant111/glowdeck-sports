#include "GoalEngine.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
namespace {
struct Light : gd::LightPort {
 bool saved=false; unsigned started=0,restored=0;uint32_t color=0;
 bool startColor(uint32_t c) override {if(saved)return false;saved=true;++started;color=c;return true;}
 bool restore() override {if(!saved)return true;saved=false;++restored;return true;}
 bool needsRestore() const override {return saved;}
};
gd::Game game(){gd::Game g;g.id=2026020001UL;std::strncpy(g.state,"LIVE",11);return g;}
gd::GoalFeed feed(uint32_t latest,uint32_t goal=0,uint32_t team=0){
 gd::GoalFeed f;f.valid=true;f.gameId=2026020001UL;f.homeId=22;f.awayId=11;
 f.latestEventId=latest;if(goal){f.count=1;f.goals[0].id=goal;f.goals[0].ownerId=team;}return f;
}
}
int main() {
 gd::GoalEngine e; Light l;gd::ReactionController reactions(e,l);
 e.selectGame(2026020001UL);e.setMode(gd::CelebrationMode::Both);e.setDelay(15);
 auto g=game();if(!e.arm(&g,1000))return EXIT_FAILURE;
 e.observeFeed(feed(2),1000); // initial baseline, no WLED calls
 e.observeFeed(feed(3,3,22),2000);
 reactions.setTeamColors(0x6CACE4,0xFFB81C);
 reactions.service(16000,true);reactions.service(17000,true);reactions.service(25000,true);
 bool pass=l.started==1 && l.restored==1 && l.color==0xFFB81C && !l.saved;
 std::cout<<"GameDay v0.3 event-ID simulation: scenes="<<l.started
  <<" restores="<<l.restored<<" home-color=0x"<<std::hex<<l.color<<std::dec
  <<" -> "<<(pass?"PASS":"FAIL")<<"\n";
 return pass?EXIT_SUCCESS:EXIT_FAILURE;
}
