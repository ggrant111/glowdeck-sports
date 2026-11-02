package com.gkowdeck.gameday.firetv;
final class SportEventTracker {
    static final class Cue {final int kind,owner;Cue(int k,int o){kind=k;owner=o;}}
    private int boundary;private boolean baseline,wasFinal,finalDelivered;
    void reset(){baseline=false;boundary=0;wasFinal=false;finalDelivered=false;}
    Cue update(NhlApi.Pbp pbp,int mask,int mode){
        int latest=0,previous=-1;Cue result=null;
        for(NhlApi.Play p:pbp.plays){
            latest=Math.max(latest,p.eventId);int kind=-1,owner=0;
            if("period-start".equals(p.type))kind=EventEffects.periodStart(p.periodType);
            else if("period-end".equals(p.type))kind=EventEffects.PERIOD_END;
            else if("game-end".equals(p.type))kind=EventEffects.GAME_END;
            int advantage=EventEffects.advantage(p.situation);
            if(kind<0&&advantage>0&&previous>=0&&advantage!=previous){kind=EventEffects.POWER_PLAY;owner=advantage==2?pbp.homeId:pbp.awayId;}
            if(advantage>=0)previous=advantage;
            if(kind==EventEffects.GAME_END && finalDelivered)continue;
            if(!baseline||p.eventId<=boundary||kind<0||(mask&(1<<kind))==0)continue;
            if(owner>0&&CelebrationSelection.acceptedOwner(owner,pbp.awayId,pbp.homeId,mode)==0)continue;
            if(result==null||EventEffects.priority(kind)>EventEffects.priority(result.kind))result=new Cue(kind,owner);
        }
        if(baseline && !finalDelivered && pbp.finished() && !wasFinal && (mask&(1<<EventEffects.GAME_END))!=0)result=new Cue(EventEffects.GAME_END,0);
        wasFinal=pbp.finished();
        if(!baseline||latest<boundary)result=null;
        if(result!=null && result.kind==EventEffects.GAME_END)finalDelivered=true;
        if(!baseline && pbp.finished())finalDelivered=true;
        boundary=latest;baseline=true;return result;
    }
}
