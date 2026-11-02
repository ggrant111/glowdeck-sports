package com.gkowdeck.gameday.firetv;

import org.junit.Test;
import static org.junit.Assert.*;
import java.util.Arrays;

public class MonitorRulesTest {
    @Test public void cannotReactIfDisarmedOrGameEnded() {
        assertFalse(MonitorRules.shouldCelebrate(false,true,false,false,4,4,90_000,0));
        assertFalse(MonitorRules.shouldCelebrate(true,false,false,false,4,4,90_000,0));
    }
    @Test public void onlyFavoriteOwnGoalQualifies() {
        assertTrue(MonitorRules.shouldCelebrate(true,true,false,false,4,4,90_000,0));
        assertFalse(MonitorRules.shouldCelebrate(true,true,false,false,3,4,90_000,0));
        assertFalse(MonitorRules.shouldCelebrate(true,true,false,false,0,4,90_000,0));
    }
    @Test public void noActionWhenRestoreOrSceneBusy() {
        assertFalse(MonitorRules.shouldCelebrate(true,true,true,false,4,4,90_000,0));
        assertFalse(MonitorRules.shouldCelebrate(true,true,false,true,4,4,90_000,0));
    }
    @Test public void cooldownRequiresTwentySeconds() {
        assertFalse(MonitorRules.shouldCelebrate(true,true,false,false,4,4,39_999,20_000));
        assertTrue(MonitorRules.shouldCelebrate(true,true,false,false,4,4,40_000,20_000));
    }
    @Test public void staleGapIsRebaselined() {
        assertTrue(MonitorRules.needsBaseline(100_000,0));
        assertFalse(MonitorRules.needsBaseline(100_000,50_000));
        assertTrue(MonitorRules.needsBaseline(130_001,50_000));
        assertTrue(MonitorRules.needsBaseline(40_000,50_000));
    }
    @Test public void delayIsCapped() {
        assertEquals(0,MonitorRules.delaySeconds(-100));
        assertEquals(90,MonitorRules.delaySeconds(100));
        assertEquals(45,MonitorRules.delaySeconds(45));
    }
    @Test public void historicalGoalIsBaselineOnly() {
        GoalTracker t=new GoalTracker();
        assertEquals(0,t.update(Arrays.asList(new GoalTracker.Event(11,4,true))));
        assertEquals(0,t.update(Arrays.asList(new GoalTracker.Event(11,4,true))));
        assertEquals(4,t.update(Arrays.asList(new GoalTracker.Event(11,4,true),new GoalTracker.Event(12,4,true))));
    }
}
