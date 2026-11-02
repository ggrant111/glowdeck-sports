package com.gkowdeck.gameday.firetv;

import android.app.*;
import android.content.*;
import android.os.*;
import org.json.*;
import java.util.*;
import java.util.concurrent.*;

/** Four isolated feeds, two journaled targets. No automatic re-arming after process restart. */
public final class FollowService extends Service {
    static final String START="follow.START",STOP="follow.STOP",RECOVER="follow.RECOVER";
    private static volatile boolean busy=false;
    static boolean isBusy(){return busy;}
    static boolean hasRecovery(Context c){SharedPreferences p=c.getSharedPreferences("gameday-follow",0);return p.contains("journal0")||p.contains("journal1");}
    static volatile String status="Stopped";
    static final String[] gameStatus={"Stopped","Stopped","Stopped","Stopped"};
    private final ScheduledExecutorService worker=Executors.newSingleThreadScheduledExecutor();
    private SharedPreferences prefs;private FollowConfig config=new FollowConfig();private GameAudio audio;
    private boolean running=false;private volatile boolean stopRequested=false;
    private long nextPoll;private int cursor;
    private final FollowQueue sounds=new FollowQueue();
    private static final class Feed {
        final GoalTracker goals=new GoalTracker();final SportEventTracker sports=new SportEventTracker();
        boolean paused=true,waiting=false,live=false,done=false;long fresh,finalAt,cooldown,due;
        FollowQueue.Event pending;SportEventTracker.Cue finalCue;
    }
    private static final class Target {final DdpStream stream=new DdpStream();final FollowQueue queue=new FollowQueue();long end,retry;boolean active,recovering;}
    private final Feed[] feeds={new Feed(),new Feed(),new Feed(),new Feed()};
    private final Target[] targets={new Target(),new Target()};
    @Override public void onCreate(){super.onCreate();prefs=getSharedPreferences("gameday-follow",0);audio=new GameAudio(this);
        if(Build.VERSION.SDK_INT>=26)((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).createNotificationChannel(new NotificationChannel("gameday_follow","Followed games",NotificationManager.IMPORTANCE_LOW));
    }
    private Notification notification(){
        int flags=PendingIntent.FLAG_UPDATE_CURRENT|(Build.VERSION.SDK_INT>=23?PendingIntent.FLAG_IMMUTABLE:0);
        Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(this,"gameday_follow"):new Notification.Builder(this);
        return b.setSmallIcon(android.R.drawable.ic_media_play).setContentTitle("GlowDeck followed games").setContentText(status).setOngoing(true).setOnlyAlertOnce(true)
            .setContentIntent(PendingIntent.getActivity(this,401,new Intent(this,MainActivity.class),flags))
            .addAction(android.R.drawable.ic_delete,"STOP all",PendingIntent.getService(this,402,new Intent(this,FollowService.class).setAction(STOP),flags)).build();
    }
    private void publish(String text){status=text;((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).notify(1228,notification());}
    @Override public int onStartCommand(Intent intent,int flags,int id){
        String action=intent==null?STOP:intent.getAction();
        if(STOP.equals(action)){stopRequested=true;if(busy)worker.execute(this::stopMonitoring);else stopSelf();return START_NOT_STICKY;}
        if(busy)return START_NOT_STICKY;
        busy=true;startForeground(1228,notification());
        worker.execute(()->{
            try{
                config=FollowConfig.parse(prefs.getString("config","{}"));
                if(START.equals(action)){
                    if(GameDayService.isRunning()||GameDayService.isStopping()||getSharedPreferences("gameday-lab",0).contains("restoreJson"))throw new Exception("Stop single-game monitoring and restore first");
                    for(int t=0;t<2;t++)restore(t);
                    config.validate();int used=0;for(FollowConfig.Game g:config.games)used|=g.outputs;
                    for(int t=0;t<2;t++)if((used&(1<<t))!=0)WledApi.info(config.targets[t]);
                    if(!stopRequested){running=true;getSharedPreferences("gameday-monitor",0).edit().putBoolean("resumeArmed",false).commit();publish("Monitoring "+config.games.size()+" followed games");}
                }
            }catch(Exception ex){publish("Stopped: "+ex.getMessage());}
            worker.scheduleWithFixedDelay(this::tick,0,200,TimeUnit.MILLISECONDS);
        });return START_NOT_STICKY;
    }
    private long now(){return SystemClock.elapsedRealtime();}
    private void drop(int i){feeds[i].pending=null;feeds[i].finalCue=null;sounds.removeGame(i);for(Target t:targets)t.queue.removeGame(i);}
    private void stopMonitoring(){running=false;audio.stop();sounds.clear();for(int i=0;i<4;i++){drop(i);gameStatus[i]="Stopped";}for(Target t:targets){t.active=false;t.retry=0;t.stream.close();}publish("Stopped; restoring targets");}
    private void restore(int index)throws Exception {
        Target t=targets[index];t.stream.close();String key="journal"+index;
        if(!prefs.contains(key))return;
        JSONObject j=new JSONObject(prefs.getString(key,""));String ip=j.getString("ip");
        if(!AddressValidator.isPrivateLanIpv4(ip))throw new Exception("Invalid recovery IP");
        WledApi.send(ip,j.getJSONObject("state").put("live",false));
        if(!prefs.edit().remove(key).commit())throw new Exception("Cannot clear restoration journal");
    }
    private void serviceTargets(){
        for(int i=0;i<2;i++){Target t=targets[i];long now=now();t.queue.expire(now);
            if(t.active&&now<t.end)continue;if(now<t.retry)continue;t.active=false;
            if(t.recovering){try{restore(i);t.recovering=false;t.retry=0;}catch(Exception ex){t.retry=now()+3000;}continue;}
            FollowQueue.Event e=running&&!stopRequested?t.queue.pop(now):null;
            if(e!=null&&(feeds[e.game].paused||feeds[e.game].done||now-feeds[e.game].fresh>75000))e=null;
            try{
                if(e==null){restore(i);continue;}
                FollowConfig.Game g=config.games.get(e.game);String ip=config.targets[i],key="journal"+i;
                t.stream.close();boolean continuing=prefs.contains(key);
                if(continuing){JSONObject saved=new JSONObject(prefs.getString(key,""));if(!ip.equals(saved.getString("ip")))throw new Exception("Recovery target mismatch");}
                int pixels=0;
                if(g.ddp){if(continuing){pixels=WledApi.info(ip).getJSONObject("leds").getInt("count");if(pixels<1||pixels>600)throw new Exception("DDP requires 1-600 LEDs");}else pixels=DdpScene.count(ip);}
                if(!continuing){JSONObject saved=new JSONObject().put("ip",ip).put("state",WledApi.snapshotRestore(WledApi.state(ip)));
                    if(!prefs.edit().putString(key,saved.toString()).commit())throw new Exception("Cannot save original scene");}
                if(stopRequested){restore(i);continue;}
                if(g.ddp)DdpScene.start(t.stream,ip,pixels,e.kind,NhlApi.teamColor(e.team));
                else WledApi.send(ip,WledApi.previewColor(NhlApi.teamColor(e.team),160).put("live",false));
                t.active=true;t.end=now()+8000;t.retry=0;
            }catch(Exception ex){t.queue.clear();t.recovering=true;t.retry=now()+3000;t.stream.close();publish("Target "+(i==0?"A":"B")+": "+ex.getMessage()+"; retrying restoration");}
        }
    }
    private void poll(int i){
        Feed f=feeds[i];if(f.done)return;FollowConfig.Game g=config.games.get(i);
        try{
            NhlApi.Pbp p=NhlApi.plays(g.id);long now=now();if(stopRequested)return;
            if(p.awayId<=0||p.homeId<=0)throw new Exception("Missing team IDs");
            if("CNCL".equals(p.state)||(p.finished()&&!f.live)){f.done=true;drop(i);gameStatus[i]="Ended / cancelled";return;}
            boolean waiting="FUT".equals(p.state)||"PRE".equals(p.state)||"PPD".equals(p.state);
            if(!waiting&&!p.finished()&&!"LIVE".equals(p.state)&&!"CRIT".equals(p.state))throw new Exception("Feed unavailable");
            List<GoalTracker.Event> events=new ArrayList<>();for(NhlApi.Play play:p.plays)events.add(new GoalTracker.Event(play.eventId,play.ownerId,play.goal()));
            int mask=g.ddp?g.mask:1;
            if(waiting){drop(i);f.goals.reset();f.goals.update(events);f.sports.reset();f.sports.update(p,mask,g.mode);f.waiting=true;f.paused=false;f.fresh=now;gameStatus[i]="Waiting for start";return;}
            if(!p.finished())f.live=true;
            if(p.finished()&&f.finalAt==0)f.finalAt=now;
            if(f.finalAt>0&&now-f.finalAt>125000){f.done=true;drop(i);gameStatus[i]="Finished";return;}
            if(f.paused||now-f.fresh>75000){drop(i);f.goals.reset();f.goals.update(events);f.sports.reset();f.sports.update(p,mask,g.mode);f.paused=false;}
            else {
                SportEventTracker.Cue cue=f.sports.update(p,mask,g.mode);
                if(cue!=null&&cue.kind==EventEffects.GAME_END)f.finalCue=cue;
                if(f.finalCue!=null)cue=f.finalCue;
                int owner=CelebrationSelection.acceptedOwner(f.goals.update(events),p.awayId,p.homeId,g.mode);
                if(owner>0&&(mask&1)!=0)cue=new SportEventTracker.Cue(EventEffects.GOAL,owner);
                if(cue!=null&&f.pending==null&&now>=f.cooldown){String team=cue.owner==p.homeId?g.home:cue.owner==p.awayId?g.away:g.mode==CelebrationSelection.HOME?g.home:g.away;
                    f.pending=new FollowQueue.Event(i,cue.kind,team,now);f.due=now+g.delay*1000L;if(cue.kind==EventEffects.GAME_END)f.finalCue=null;}
            }
            f.fresh=now;f.waiting=false;gameStatus[i]=p.finished()?"Final effects":g.away+" @ "+g.home+" • live";
        }catch(Exception ex){f.paused=true;drop(i);gameStatus[i]="Reconnecting; old events skipped";}
    }
    private void tick(){
        try{
            if(stopRequested&&running)stopMonitoring();
            if(!getSharedPreferences("gameday-audio",0).getBoolean("enabled",false)){audio.stop();sounds.clear();}
            serviceTargets();
            if(running&&!stopRequested){
                if(now()>=nextPoll){nextPoll=now()+6250;poll(cursor++%config.games.size());}
                boolean allDone=true;
                for(int i=0;i<config.games.size();i++){Feed f=feeds[i];if(!f.done)allDone=false;
                    if(now()-f.fresh>75000){f.paused=true;drop(i);}
                    if(f.pending==null||f.paused||now()<f.due)continue;
                    FollowQueue.Event old=f.pending;f.pending=null;f.cooldown=now()+20000;
                    FollowQueue.Event e=new FollowQueue.Event(i,old.kind,old.team,now());FollowConfig.Game g=config.games.get(i);
                    for(int t=0;t<2;t++)if((g.outputs&(1<<t))!=0)targets[t].queue.push(e,now());
                    if(g.sound&&e.kind==EventEffects.GOAL)sounds.push(e,now());
                    if(g.display)gameStatus[i]=EventEffects.NAMES[e.kind]+": "+e.team;
                }
                if(!audio.isPlaying()){FollowQueue.Event e=sounds.pop(now());if(e!=null)audio.play(e.team,false);}
                if(allDone)stopMonitoring();serviceTargets();
            }
            if(!running&&!hasRecovery(this)){busy=false;stopForeground(true);stopSelf();}
        }catch(Exception ex){publish("Follow error: "+ex.getMessage());}
    }
    @Override public void onDestroy(){stopRequested=true;running=false;busy=false;for(Target t:targets)t.stream.close();if(audio!=null)audio.stop();worker.shutdownNow();super.onDestroy();}
    @Override public IBinder onBind(Intent i){return null;}
}
