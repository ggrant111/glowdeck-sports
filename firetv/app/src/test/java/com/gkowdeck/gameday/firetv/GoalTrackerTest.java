package com.gkowdeck.gameday.firetv;

import org.junit.Test;
import static org.junit.Assert.*;
import java.util.Arrays;
import java.util.Collections;

public class GoalTrackerTest {
    @Test public void initialGoalsAreBaselineNotTriggers(){GoalTracker x=new GoalTracker();assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(1,1,true))));assertTrue(x.isBaselined());assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(1,1,true))));}
    @Test public void uniqueGoalTriggersOnce(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());assertEquals(24,x.update(Arrays.asList(new GoalTracker.Event(42,24,true))));assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(42,24,true))));}
    @Test public void multiGoalJumpIsSuppressed(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(5,3,true),new GoalTracker.Event(6,3,true))));assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(5,3,true))));}
    @Test public void nonGoalIgnored(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(4,9,false))));}
    @Test public void resetRebaselines(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());x.reset();assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(5,9,true))));}
    @Test public void missingOwnerNeverTriggers(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(1,0,true))));}
    @Test public void duplicateIdsDoNotReplay(){GoalTracker x=new GoalTracker();x.update(Collections.emptyList());assertEquals(3,x.update(Arrays.asList(new GoalTracker.Event(1,3,true))));assertEquals(0,x.update(Arrays.asList(new GoalTracker.Event(1,3,true),new GoalTracker.Event(1,3,true))));}
    @Test public void privateHostsOnly(){assertTrue(AddressValidator.isPrivateLanIpv4("192.168.1.71"));assertTrue(AddressValidator.isPrivateLanIpv4("10.0.0.1"));assertTrue(AddressValidator.isPrivateLanIpv4("172.16.0.2"));assertFalse(AddressValidator.isPrivateLanIpv4("8.8.8.8"));assertFalse(AddressValidator.isPrivateLanIpv4("192.168.1.255"));assertFalse(AddressValidator.isPrivateLanIpv4("192.168.1.0"));assertFalse(AddressValidator.isPrivateLanIpv4("localhost"));assertFalse(AddressValidator.isPrivateLanIpv4("192.168.1.71:80"));}
}
