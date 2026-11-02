package com.gkowdeck.gameday.firetv;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Only previously unseen, unambiguous goal events after a confirmed baseline qualify. */
public final class GoalTracker {
    private final Set<Integer> seen=new HashSet<>();
    private boolean baselined=false;
    public void reset(){seen.clear();baselined=false;}
    public boolean isBaselined(){return baselined;}
    /** Returns owner team ID iff exactly one new goal exists and ownership is known. */
    public int update(List<Event> plays) {
        if(!baselined){for(Event p:plays)if(p.goal&&p.id>0)seen.add(p.id);baselined=true;return 0;}
        int newGoals=0,owner=0;
        for(Event p:plays)if(p.goal&&p.id>0&&!seen.contains(p.id)) {
            seen.add(p.id);newGoals++;owner=p.owner;
        }
        return newGoals==1?owner:0;
    }
    public static final class Event {
        public final int id,owner;public final boolean goal;
        public Event(int id,int owner,boolean goal){this.id=id;this.owner=owner;this.goal=goal;}
    }
}
