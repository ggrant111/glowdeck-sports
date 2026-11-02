package com.gkowdeck.gameday.firetv;

import org.junit.Test;
import static org.junit.Assert.*;
import java.util.Arrays;
import java.util.Collections;

public class CelebrationSelectionTest {
    @Test public void awayOnlyIgnoresHome() {
        assertEquals(11,CelebrationSelection.acceptedOwner(11,11,22,CelebrationSelection.AWAY));
        assertEquals(0,CelebrationSelection.acceptedOwner(22,11,22,CelebrationSelection.AWAY));
    }
    @Test public void homeOnlyIgnoresAway() {
        assertEquals(22,CelebrationSelection.acceptedOwner(22,11,22,CelebrationSelection.HOME));
        assertEquals(0,CelebrationSelection.acceptedOwner(11,11,22,CelebrationSelection.HOME));
    }
    @Test public void bothAcceptsEachOwnerInTurn() {
        assertEquals(11,CelebrationSelection.acceptedOwner(11,11,22,CelebrationSelection.BOTH));
        assertEquals(22,CelebrationSelection.acceptedOwner(22,11,22,CelebrationSelection.BOTH));
        assertEquals("OTT + ANA",CelebrationSelection.label(CelebrationSelection.BOTH,"OTT","ANA"));
    }
    @Test public void rejectsUnknownOrMalformedOwnership() {
        for(int mode=0;mode<=2;mode++){
            assertEquals(0,CelebrationSelection.acceptedOwner(0,11,22,mode));
            assertEquals(0,CelebrationSelection.acceptedOwner(33,11,22,mode));
            assertEquals(0,CelebrationSelection.acceptedOwner(11,11,11,mode));
            assertEquals(0,CelebrationSelection.acceptedOwner(11,0,22,mode));
        }
    }
    @Test public void unexpectedStoredModeFallsBackToAwayOnly() {
        assertEquals(CelebrationSelection.AWAY,CelebrationSelection.normalize(-3));
        assertEquals(CelebrationSelection.AWAY,CelebrationSelection.normalize(999));
        assertEquals(0,CelebrationSelection.acceptedOwner(22,11,22,999));
    }
    @Test public void existingDedupeAndSafetyStillWorkWithBoth() {
        GoalTracker tracker=new GoalTracker();
        tracker.update(Collections.singletonList(new GoalTracker.Event(1,11,true)));
        int goalOwner=tracker.update(Arrays.asList(new GoalTracker.Event(1,11,true),new GoalTracker.Event(2,22,true)));
        assertEquals(22,goalOwner);
        assertTrue(MonitorRules.shouldCelebrate(true,true,false,false,goalOwner,
            CelebrationSelection.acceptedOwner(goalOwner,11,22,CelebrationSelection.BOTH),40_000,0));
        assertEquals(0,tracker.update(Arrays.asList(new GoalTracker.Event(1,11,true),new GoalTracker.Event(2,22,true))));
        assertFalse(MonitorRules.shouldCelebrate(true,true,false,true,goalOwner,
            CelebrationSelection.acceptedOwner(goalOwner,11,22,CelebrationSelection.BOTH),40_000,0));
    }
    @Test public void multipleNewGoalsSuppressedEvenWhenBothSelected() {
        GoalTracker tracker=new GoalTracker();
        tracker.update(Collections.emptyList());
        assertEquals(0,tracker.update(Arrays.asList(new GoalTracker.Event(3,11,true),new GoalTracker.Event(4,22,true))));
    }
}
