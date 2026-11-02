package com.gkowdeck.gameday.firetv;
import org.junit.Test;
import static org.junit.Assert.*;
public class SportEventTrackerTest {
 private NhlApi.Pbp feed(){NhlApi.Pbp f=new NhlApi.Pbp();f.homeId=2;f.awayId=1;f.state="LIVE";return f;}
 private void add(NhlApi.Pbp f,int id,String type,String pt,String code){NhlApi.Play p=new NhlApi.Play();p.eventId=id;p.type=type;p.periodType=pt;p.situation=code;f.plays.add(p);}
 @Test public void baselineDedupeAndOT(){SportEventTracker t=new SportEventTracker();NhlApi.Pbp f=feed();add(f,1,"period-start","REG","1551");assertNull(t.update(f,127,0));add(f,2,"period-start","OT","1331");assertEquals(EventEffects.OVERTIME,t.update(f,127,0).kind);assertNull(t.update(f,127,0));}
 @Test public void powerPlayExcludesPulledGoalieAndWrongTeam(){SportEventTracker t=new SportEventTracker();NhlApi.Pbp f=feed();add(f,1,"faceoff","REG","1551");t.update(f,127,0);add(f,2,"shot-on-goal","REG","1550");assertNull(t.update(f,127,0));add(f,3,"faceoff","REG","1451");assertNull(t.update(f,127,0));add(f,4,"faceoff","REG","1541");assertEquals(1,t.update(f,127,0).owner);}
 @Test public void finalTransitionOnceAndRecoveryBaseline(){SportEventTracker t=new SportEventTracker();NhlApi.Pbp f=feed();add(f,1,"faceoff","REG","1551");t.update(f,127,2);f.state="OFF";assertEquals(EventEffects.GAME_END,t.update(f,127,2).kind);assertNull(t.update(f,127,2));add(f,2,"game-end","REG","1551");assertNull(t.update(f,127,2));t.reset();assertNull(t.update(f,127,2));}
 @Test public void disabledAndShootout(){SportEventTracker t=new SportEventTracker();NhlApi.Pbp f=feed();add(f,1,"faceoff","REG","1551");t.update(f,127,2);add(f,2,"period-start","SO","");assertNull(t.update(f,1,2));add(f,3,"period-start","SO","");assertEquals(EventEffects.SHOOTOUT,t.update(f,127,2).kind);NhlApi.Play p=f.plays.get(2);p.type="goal";assertFalse(p.goal());}

 @Test public void pregameBaselineCapturesOpeningPeriodWithoutReplay(){
  SportEventTracker t=new SportEventTracker();NhlApi.Pbp f=feed();f.state="PRE";
  assertNull(t.update(f,127,2));f.state="LIVE";add(f,1,"period-start","REG","1551");
  assertEquals(EventEffects.PERIOD_START,t.update(f,127,2).kind);assertNull(t.update(f,127,2));
  t.reset();add(f,2,"period-start","OT","1331");assertNull(t.update(f,127,2));
 }
 @Test public void upcomingStates(){NhlApi.Game g=new NhlApi.Game();g.state="FUT";assertTrue(g.upcoming());g.state="PPD";assertTrue(g.upcoming());g.state="OFF";assertFalse(g.upcoming());g.state="CNCL";assertFalse(g.upcoming());}
}
