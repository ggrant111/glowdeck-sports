package com.gkowdeck.gameday.firetv;

/** Which goal owner's event is eligible to show a scene. No default auto-arming. */
final class CelebrationSelection {
    static final int AWAY=0;
    static final int HOME=1;
    static final int BOTH=2;
    static int normalize(int mode) { return mode==HOME||mode==BOTH ? mode : AWAY; }
    /** A valid selected owner ID, or 0 to ignore. Unknown/ambiguous IDs never fire. */
    static int acceptedOwner(int goalOwner,int awayId,int homeId,int mode) {
        if(goalOwner <= 0 || awayId <= 0 || homeId <= 0 || awayId==homeId)return 0;
        if(goalOwner!=awayId && goalOwner!=homeId)return 0;
        int value=normalize(mode);
        if(value==BOTH || (value==AWAY && goalOwner==awayId) || (value==HOME && goalOwner==homeId))
            return goalOwner;
        return 0;
    }
    static String label(int mode,String away,String home) {
        switch(normalize(mode)) {
            case HOME: return home;
            case BOTH: return away+" + "+home;
            default: return away;
        }
    }
}
