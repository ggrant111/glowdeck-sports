package com.gkowdeck.gameday.firetv;
import java.util.*;
/** Stable priority queue, eight entries, twenty seconds after configured TV delay. */
final class FollowQueue {
    static final long TTL=20000;
    static final class Event {
        final int game,kind;final String team;final long at;
        Event(int game,int kind,String team,long at){this.game=game;this.kind=kind;this.team=team;this.at=at;}
    }
    private final List<Event> events=new ArrayList<>();
    int size(){return events.size();}
    void clear(){events.clear();}
    void removeGame(int game){for(int i=events.size()-1;i>=0;i--)if(events.get(i).game==game)events.remove(i);}
    void expire(long now){for(int i=events.size()-1;i>=0;i--)if(now-events.get(i).at>TTL)events.remove(i);}
    boolean push(Event e,long now){expire(now);if(events.size()==8){int low=0;for(int i=1;i<events.size();i++)if(EventEffects.priority(events.get(i).kind)<EventEffects.priority(events.get(low).kind))low=i;
        if(EventEffects.priority(e.kind)<=EventEffects.priority(events.get(low).kind))return false;events.remove(low);}
        events.add(e);return true;}
    Event pop(long now){expire(now);if(events.isEmpty())return null;int best=0;for(int i=1;i<events.size();i++)if(EventEffects.priority(events.get(i).kind)>EventEffects.priority(events.get(best).kind))best=i;return events.remove(best);}
}
