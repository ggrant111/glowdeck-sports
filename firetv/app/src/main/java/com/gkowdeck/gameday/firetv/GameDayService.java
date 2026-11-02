package com.gkowdeck.gameday.firetv;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import org.json.JSONObject;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * Opt-in foreground NHL goal monitor. Runs independently of MainActivity so the viewer can
 * open streaming apps. The system may still stop the process; no unattended guarantees.
 * Optional explicit reboot resume; ordinary service restart remains START_NOT_STICKY.
 */
public final class GameDayService extends Service {
    public static final String START = "com.gkowdeck.gameday.firetv.START_MONITOR";
    public static final String STOP = "com.gkowdeck.gameday.firetv.STOP_MONITOR";
    private static final String CHANNEL = "gameday_monitor";
    private static final int NOTIFICATION_ID = 1227;
    private static volatile boolean running = false;
    private static volatile boolean stopping = false;
    private final ScheduledExecutorService worker = Executors.newSingleThreadScheduledExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());
    private final GoalTracker tracker = new GoalTracker();
    private final SportEventTracker sports = new SportEventTracker();
    private final DdpStream ddp = new DdpStream();
    private boolean useDdp;
    private int eventMask=1;
    private long finalSeenAt=0;
    private SportEventTracker.Cue deferredFinal;
    private SharedPreferences monitor, lab;
    private GameAudio sound;
    private long gameId, lastSuccessfulPoll, lastScoreCheck, lastEffectAt;
    private String away, home, ip;
    private boolean baselined, paused, waiting, observedLive;
    private int celebrationMode;
    private int delaySec;
    private ScheduledFuture<?> pollTask, queuedGoal, restoreTask;
    private String state = "STARTING";

    public static boolean isRunning() { return running; }
    public static boolean isStopping() { return stopping; }

    @Override public void onCreate() {
        super.onCreate();
        monitor = getSharedPreferences("gameday-monitor", MODE_PRIVATE);
        lab = getSharedPreferences("gameday-lab", MODE_PRIVATE);
        sound=new GameAudio(this);
        useDdp=lab.getBoolean("ddp",false);
        eventMask=useDdp?lab.getInt("eventMask",127)&127:1;
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel channel = new NotificationChannel(CHANNEL, "GlowDeck GameDay monitoring", NotificationManager.IMPORTANCE_LOW);
            channel.setDescription("NHL game monitoring and WLED reactions; choose STOP to disarm");
            ((NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE)).createNotificationChannel(channel);
        }
    }

    private PendingIntent activityIntent() {
        Intent i = new Intent(this, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        return PendingIntent.getActivity(this, 301, i, PendingIntent.FLAG_UPDATE_CURRENT | immutable());
    }
    private PendingIntent stopIntent() {
        Intent i = new Intent(this, GameDayService.class).setAction(STOP);
        return PendingIntent.getService(this, 302, i, PendingIntent.FLAG_UPDATE_CURRENT | immutable());
    }
    private int immutable() { return Build.VERSION.SDK_INT >= 23 ? PendingIntent.FLAG_IMMUTABLE : 0; }

    private Notification notification(String text) {
        Notification.Builder n = Build.VERSION.SDK_INT >= 26 ? new Notification.Builder(this, CHANNEL) : new Notification.Builder(this);
        return n.setSmallIcon(android.R.drawable.ic_media_play)
                .setContentTitle("GlowDeck GameDay • " + state)
                .setContentText(text).setContentIntent(activityIntent())
                .setOngoing(true).setOnlyAlertOnce(true)
                .addAction(android.R.drawable.ic_delete, "STOP monitoring", stopIntent())
                .build();
    }
    private void publish(String status, String description) {
        state = status;
        monitor.edit().putString("mode", status).putString("detail", description)
                .putLong("updatedAt", System.currentTimeMillis()).apply();
        Log.i("GlowDeckMonitor", status + ": " + description);
        if (running) ((NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE))
                .notify(NOTIFICATION_ID, notification(description));
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null || STOP.equals(intent.getAction())) {
            stopping = true;
            worker.execute(() -> shutDown("Stopped by user"));
            return START_NOT_STICKY;
        }
        if (!START.equals(intent.getAction())) { stopSelf(); return START_NOT_STICKY; }
        if (running || stopping) return START_NOT_STICKY;
        if(FollowService.isBusy()||FollowService.hasRecovery(this)){stopSelf();return START_NOT_STICKY;}
        running = true;
        gameId = intent.getLongExtra("gameId", 0);
        away = intent.getStringExtra("away"); home = intent.getStringExtra("home");
        ip = intent.getStringExtra("wledIp");
        celebrationMode = CelebrationSelection.normalize(intent.hasExtra("celebrationMode")
                ? intent.getIntExtra("celebrationMode", CelebrationSelection.AWAY)
                : (intent.getBooleanExtra("favoriteHome", false) ? CelebrationSelection.HOME : CelebrationSelection.AWAY));
        delaySec = MonitorRules.delaySeconds(intent.getIntExtra("delaySec", 0));
        monitor.edit().putLong("gameId", gameId).putString("away", away).putString("home", home)
                .putString("wledIp", ip).putInt("celebrationMode", celebrationMode).putInt("delaySec",delaySec).apply();
        // Android 8+ requires foreground promotion immediately, BEFORE network access.
        state = "STARTING";
        startForeground(NOTIFICATION_ID, notification("Checking NHL baseline and WLED"));
        publish("STARTING", "Checking NHL baseline and WLED");
        monitor.edit().putBoolean("resumeArmed",true).apply();
        worker.execute(this::startMonitor);
        return START_NOT_STICKY; // don't silently re-arm after an OS kill
    }

    private void startMonitor() {
        try {
            paused=true;
            if (gameId <= 0 || !AddressValidator.isPrivateLanIpv4(ip) || away == null || home == null)
                throw new Exception("Invalid game or WLED selection");
            if (stopping) return;
            if (lab.contains("restoreJson")) {
                publish("RESTORING", "Recovering interrupted WLED scene before monitoring");
                restoreSavedState();
            }
            WledApi.info(ip); // required: selected target answers as WLED
            pollSafe();
            if(stopping)return;
            pollTask = worker.scheduleWithFixedDelay(this::pollSafe, 25, 25, TimeUnit.SECONDS);
        } catch (Exception ex) {
            paused=true;publish("PAUSED","Waiting for WLED/network: "+message(ex));
            if(!stopping)pollTask=worker.schedule(this::startMonitor,30,TimeUnit.SECONDS);
        }
    }
    private static List<GoalTracker.Event> events(NhlApi.Pbp pbp) {
        List<GoalTracker.Event> e = new ArrayList<>();
        for (NhlApi.Play p : pbp.plays) if (p.eventId > 0) e.add(new GoalTracker.Event(p.eventId,p.ownerId,p.goal()));
        return e;
    }
    private boolean gameIsLive(NhlApi.Board scores) {
        for (NhlApi.Game g : scores.games) if (g.id == gameId) return g.live();
        return false;
    }
    private void pollSafe() {
        if (stopping || !running) return;
        try {
            long now = System.currentTimeMillis();
            NhlApi.Pbp pbp = NhlApi.plays(gameId);
            if (pbp.awayId <= 0 || pbp.homeId <= 0) throw new Exception("NHL team IDs unavailable");
            if("CNCL".equals(pbp.state)){shutDown("Game cancelled");return;}
            if("FUT".equals(pbp.state)||"PRE".equals(pbp.state)||"PPD".equals(pbp.state)) {
                waiting=true;cancelGoal();
                tracker.reset();tracker.update(events(pbp));sports.reset();sports.update(pbp,eventMask,celebrationMode);
                baselined=true;paused=false;lastSuccessfulPoll=now;
                publish("WAITING",away+" @ "+home+("PPD".equals(pbp.state)?" • postponed; waiting for reschedule":" • armed, waiting for game start"));
                return;
            }
            if(pbp.finished()&&!useDdp){shutDown("Game ended; lights restored");return;}
            if(pbp.finished()&&!observedLive){shutDown("Saved game already finished; no events replayed");return;}
            if("LIVE".equals(pbp.state)||"CRIT".equals(pbp.state))observedLive=true;
            if(waiting){waiting=false;publish("ARMED",away+" @ "+home+" • monitoring live");}
            now = System.currentTimeMillis();
            if(pbp.finished() && finalSeenAt==0)finalSeenAt=now;
            if(finalSeenAt>0 && now-finalSeenAt>125000 && queuedGoal==null && restoreTask==null && deferredFinal==null){shutDown("Game ended; effects restored");return;}
            if(!pbp.finished() && !"LIVE".equals(pbp.state) && !"CRIT".equals(pbp.state))throw new Exception("Game feed not live");
            if (lab.contains("restoreJson") && restoreTask == null) {
                try { restoreSavedState(); }
                catch (Exception recovery) { publish("RESTORE NEEDED", "WLED restore failed; all new goals blocked: " + message(recovery)); return; }
            }
            if (paused || MonitorRules.needsBaseline(now, lastSuccessfulPoll) || !baselined) {
                tracker.reset(); tracker.update(events(pbp));
                sports.reset();sports.update(pbp,eventMask,celebrationMode);deferredFinal=null;
                publish("ARMED", "Feed resumed; old goals skipped, monitoring new events");
                paused = false;baselined=true;
            } else {
                SportEventTracker.Cue cue=sports.update(pbp,eventMask,celebrationMode);
                if(cue!=null&&cue.kind==EventEffects.GAME_END)deferredFinal=cue;
                if(deferredFinal!=null)cue=deferredFinal;
                int goalOwner = tracker.update(events(pbp));
                int acceptedOwner=CelebrationSelection.acceptedOwner(goalOwner,pbp.awayId,pbp.homeId,celebrationMode);
                boolean canReact = MonitorRules.shouldCelebrate(running && !stopping, true,
                        lab.contains("restoreJson"), queuedGoal != null || restoreTask != null,
                        goalOwner, acceptedOwner, now, lastEffectAt);
                if (goalOwner > 0 && !canReact)
                    Log.i("GlowDeckMonitor", "Goal ignored (team not selected/busy/cooldown/recovery)");
                if (canReact && (eventMask&1)!=0) queueGoal(goalOwner == pbp.homeId ? home : away,EventEffects.GOAL);
                else if(cue!=null && queuedGoal==null && restoreTask==null && !lab.contains("restoreJson") &&
                        (lastEffectAt==0 || now-lastEffectAt>=20_000)) {
                    String team=cue.owner==pbp.homeId?home:cue.owner==pbp.awayId?away:
                        celebrationMode==CelebrationSelection.HOME?home:away;
                    queueGoal(team,cue.kind);if(cue.kind==EventEffects.GAME_END)deferredFinal=null;
                }
            }
            lastSuccessfulPoll = now;
        } catch (Exception ex) {
            paused = true;
            cancelGoal();
            publish("PAUSED", "NHL/Wi-Fi error; new baseline on recovery: " + message(ex));
            // Keep polling while foreground service remains active. NO old goals on resumption.
        }
    }
    private void queueGoal(String team,int kind) {
        if (queuedGoal != null || stopping) return;
        publish(EventEffects.NAMES[kind]+" WAIT", team + " • " + delaySec + "s TV delay");
        queuedGoal = worker.schedule(() -> {
            queuedGoal = null;
            if (stopping || paused || !running || lab.contains("restoreJson")) return;
            try {
                // Recheck game status immediately before modifying WLED.
                NhlApi.Pbp current=NhlApi.plays(gameId);
                if("CNCL".equals(current.state)||(!useDdp&&current.finished())){shutDown("Game ended or cancelled");return;}
                if(!"LIVE".equals(current.state)&&!"CRIT".equals(current.state)&&!(useDdp&&current.finished())){
                    paused=true;publish("WAITING","Game not live; pending effect skipped");return;
                }
                lastEffectAt = System.currentTimeMillis();
                scene(team,kind);
            } catch (Exception ex) {
                paused = true;
                publish("PAUSED", "Could not apply goal scene: " + message(ex));
                if (lab.contains("restoreJson")) {
                    try { restoreSavedState(); } catch (Exception ignore) { publish("RESTORE NEEDED", "Open GameDay and restore WLED"); }
                }
            }
        }, delaySec, TimeUnit.SECONDS);
    }
    private void scene(String team,int kind) throws Exception {
        if (lab.contains("restoreJson")) throw new Exception("Prior scene must be restored first");
        int pixels=useDdp?DdpScene.count(ip):0;
        JSONObject snapshot = WledApi.snapshotRestore(WledApi.state(ip));
        if (!lab.edit().putString("restoreJson", snapshot.toString()).putString("restoreIp", ip).commit())
            throw new Exception("Cannot journal prior WLED scene");
        if(useDdp)DdpScene.start(ddp,ip,pixels,kind,NhlApi.teamColor(team));
        else WledApi.send(ip, WledApi.previewColor(NhlApi.teamColor(team),160));
        // Audio is optional and runs on its own thread. Never delay WLED restoration.
        try { if(kind==EventEffects.GOAL&&sound!=null)sound.play(team,false); }
        catch(Exception audioError){Log.w("GlowDeckAudio","Audio could not start",audioError);}
        publish(EventEffects.NAMES[kind]+" LIGHTS", team + " celebration active; restoring in 8 seconds");
        restoreTask = worker.schedule(() -> {
            restoreTask = null;
            try {
                restoreSavedState();
                if (!stopping && !paused) publish("ARMED", "Restored scene • awaiting " + CelebrationSelection.label(celebrationMode,away,home) + " goals");
            } catch (Exception ex) { publish("RESTORE NEEDED", "WLED restore failed. Open app: " + message(ex)); }
        },8,TimeUnit.SECONDS);
    }
    private void restoreSavedState() throws Exception {
        ddp.close();
        if(sound!=null)sound.stop();
        if (!lab.contains("restoreJson")) return;
        String recoveryIp = lab.getString("restoreIp", "");
        String json = lab.getString("restoreJson", "");
        if (!AddressValidator.isPrivateLanIpv4(recoveryIp)) throw new Exception("Saved restore IP invalid");
        WledApi.send(recoveryIp, new JSONObject(json).put("live",false));
        if (!lab.edit().remove("restoreJson").remove("restoreIp").commit())
            throw new Exception("Scene restored but journal could not be cleared");
    }
    private void cancelGoal() {
        if (queuedGoal != null) { queuedGoal.cancel(false); queuedGoal = null; }
    }
    private void shutDown(String why) {
        stopping = true;
        monitor.edit().putBoolean("resumeArmed",false).apply();
        if(sound!=null)sound.stop();
        cancelGoal();
        if (pollTask != null) pollTask.cancel(false);
        if (restoreTask != null) { restoreTask.cancel(false); restoreTask = null; }
        try { restoreSavedState(); }
        catch (Exception ex) { why += " • RESTORE NEEDED: " + message(ex); }
        publish("STOPPED", why);
        main.post(() -> {
            running = false; stopping = false;
            if (Build.VERSION.SDK_INT >= 24) stopForeground(STOP_FOREGROUND_REMOVE);
            else stopForeground(true);
            stopSelf();
        });
    }
    private void failAndStop(String reason) { shutDown(reason); }
    private static String message(Exception ex) { return ex.getMessage() == null ? ex.getClass().getSimpleName() : ex.getMessage(); }
    @Override public void onDestroy() {
        ddp.close();
        if(sound!=null)sound.stop();
        running = false; stopping = false;
        cancelGoal();
        if (pollTask != null) pollTask.cancel(false);
        if (restoreTask != null) restoreTask.cancel(false);
        worker.shutdownNow();
        monitor.edit().putString("mode", "STOPPED").putString("detail", "Service stopped; check WLED restore status on reopening").apply();
        super.onDestroy();
    }
    @Override public IBinder onBind(Intent intent) { return null; }
}
