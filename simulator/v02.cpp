#include "GoalEngine.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

namespace {
gd::Game game(const char *state, int away, int home) {
  gd::Game g;
  g.id=2026020001;
  std::strncpy(g.away,"UTA",4);
  std::strncpy(g.home,"BOS",4);
  std::strncpy(g.state,state,11);
  g.awayScore=(uint8_t)away; g.homeScore=(uint8_t)home;
  return g;
}
struct FakeWled : gd::LightPort {
  bool saved=false;
  unsigned start=0,restored=0;
  uint32_t color=0;
  bool startColor(uint32_t c) override { if(saved)return false; saved=true; ++start; color=c; return true; }
  bool restore() override { if(!saved)return true; saved=false; ++restored; return true; }
  bool needsRestore() const override { return saved; }
};
}
int main() {
  gd::GoalEngine e; FakeWled lights; gd::ReactionController r(e, lights);
  e.selectGame(2026020001);
  e.setFavoriteHome(false);
  e.setDelay(15);
  auto g=game("LIVE",1,0);
  if(!e.arm(&g,1000))return EXIT_FAILURE;
  g=game("LIVE",2,0);e.observe(&g,2000);r.setTeamColors(0x6CACE4,0xFFB81C);
  r.service(16000,true);r.service(17000,true);r.service(27000,true);
  e.observe(&g,28000); // duplicate poll, no second event
  bool pass=lights.start==1 && lights.restored==1 && !lights.saved;
  std::cout << "GameDay v0.2 desktop simulation: goal scenes=" << lights.start
    << " restores=" << lights.restored << " color=0x" << std::hex << lights.color << std::dec
    << " armed=" << e.armed() << " -> " << (pass?"PASS":"FAIL") << "\n";
  return pass?EXIT_SUCCESS:EXIT_FAILURE;
}
