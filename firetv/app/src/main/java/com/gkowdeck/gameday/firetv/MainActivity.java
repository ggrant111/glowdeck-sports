package com.gkowdeck.gameday.firetv;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import org.json.JSONObject;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Leanback-friendly native TV test console. No WebView, service, login, or external backend. */
public final class MainActivity extends Activity {
    private static final int BG=Color.rgb(9,19,23), PANEL=Color.rgb(26,46,54), TEAL=Color.rgb(49,131,122);
    private static final int WHITE=Color.rgb(241,249,250), MUTED=Color.rgb(169,192,199), BLUE=Color.rgb(108,172,228), GOLD=Color.rgb(255,184,28);
    private final ExecutorService io=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    private final GoalTracker goals=new GoalTracker();
    private LinearLayout root,nav,body; private TextView status,notice;private EditText ipEntry;
    private SharedPreferences prefs;
    private NhlApi.Board board; private NhlApi.Game selected;
    private final List<String> lines=new ArrayList<>();
    private List<NhlApi.Play> recentPlays=new ArrayList<>();
    private int page=0,delaySec=0;private boolean favoriteHome=false,armed=false,armPending=false;
    private boolean wledConnected=false,scoreBusy=false,pbpBusy=false,resumed=false,previewActive=false,restoreBusy=false;
    private String wledIp="192.168.1.71",wledName="Not connected",fetchStatus="Not checked",lastScoreAt="Never";
    private long lastScoresMs=0,lastPbpMs=0,lastEffectMs=0;
    private Runnable pendingGoal,scheduledRestore;
    private final Runnable tick=new Runnable(){public void run(){
        if(!resumed)return;
        long now=System.currentTimeMillis();
        if(now-lastScoresMs>60000)refreshScores(false);
        if(armed && selected!=null && selected.live() && now-lastPbpMs>=25000)checkPlays(false);
        ui.postDelayed(this,1000);
    }};

    @Override public void onCreate(Bundle state){
        super.onCreate(state);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        prefs=getSharedPreferences("gameday-lab",MODE_PRIVATE);wledIp=prefs.getString("wledIp","192.168.1.71");
        delaySec=prefs.getInt("delaySec",0);
        log("GameDay Fire TV v0.1 started. Automatic goal lighting DISARMED.");
        buildShell();render();
        if(hasJournal()){log("Found interrupted WLED preview; attempting restore before new commands.");restoreWled();}
    }
    @Override protected void onResume(){super.onResume();resumed=true;ui.removeCallbacks(tick);ui.post(tick);}
    @Override protected void onStop(){resumed=false;ui.removeCallbacks(tick);disarm("App left foreground");super.onStop();}
    @Override protected void onDestroy(){ui.removeCallbacks(tick);if(pendingGoal!=null)ui.removeCallbacks(pendingGoal);if(scheduledRestore!=null)ui.removeCallbacks(scheduledRestore);io.shutdown();super.onDestroy();}

    private int dp(int n){return (int)(getResources().getDisplayMetrics().density*n+0.5f);}
    private GradientDrawable shape(int fill,int stroke){GradientDrawable d=new GradientDrawable();d.setColor(fill);d.setCornerRadius(dp(12));if(stroke!=0)d.setStroke(dp(2),stroke);return d;}
    private LinearLayout column(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private TextView txt(String s,int sp,int color,boolean bold){TextView t=new TextView(this);t.setText(s);t.setTextSize(sp);t.setTextColor(color);if(bold)t.setTypeface(null,Typeface.BOLD);t.setPadding(dp(4),dp(5),dp(4),dp(5));return t;}
    private void buildShell(){
        root=column();root.setBackgroundColor(BG);root.setPadding(dp(35),dp(18),dp(35),dp(12));
        setContentView(root);
        LinearLayout header=row();root.addView(header);
        TextView title=txt("GLOWDECK  /  GAME DAY",28,WHITE,true);header.addView(title,new LinearLayout.LayoutParams(0,-2,1));
        status=txt("NHL: connecting",14,MUTED,false);header.addView(status);
        nav=row();nav.setPadding(0,dp(12),0,dp(10));root.addView(nav);
        ScrollView scroller=new ScrollView(this);scroller.setFillViewport(false);
        root.addView(scroller,new LinearLayout.LayoutParams(-1,0,1));body=column();body.setPadding(dp(2),dp(4),dp(2),dp(16));scroller.addView(body);
        notice=txt("D-pad: navigate  •  Select: activate  •  Back: leave",12,MUTED,false);
        root.addView(notice);
    }
    private Button action(String text,boolean active, Runnable run){
        Button b=new Button(this);b.setText(text);b.setAllCaps(false);b.setTextSize(17);b.setTextColor(WHITE);b.setPadding(dp(13),dp(8),dp(13),dp(8));
        b.setMinHeight(dp(52));b.setBackground(shape(active?TEAL:PANEL,0));
        b.setFocusable(true);b.setFocusableInTouchMode(true);
        b.setOnFocusChangeListener((v,focused)->b.setBackground(shape(active?TEAL:PANEL,focused?GOLD:0)));
        b.setOnClickListener(v->run.run());b.setTag("a:"+text);return b;
    }
    private void buttons(LinearLayout dest,Button... items){for(Button b:items){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-2,dp(54));p.setMargins(0,0,dp(10),dp(9));dest.addView(b,p);}}
    private void card(String title,String detail){LinearLayout c=column();c.setPadding(dp(15),dp(11),dp(15),dp(12));c.setBackground(shape(PANEL,0));
        LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(-1,-2);p.setMargins(0,0,0,dp(13));body.addView(c,p);
        c.addView(txt(title,20,WHITE,true));c.addView(txt(detail,16,MUTED,false));}
    private void heading(String title){body.addView(txt(title,24,WHITE,true));}
    private void subtitle(String title){body.addView(txt(title,15,MUTED,false));}
    private void render(){
        if(root==null)return;
        String focusedTag=null;View focus=getCurrentFocus();
        if(focus!=null&&focus.getTag() instanceof String)focusedTag=(String)focus.getTag();
        nav.removeAllViews();body.removeAllViews();
        status.setText("NHL: "+fetchStatus+"   |   WLED: "+(wledConnected?wledName:"not connected"));
        String[] names={"NHL SCORES","GAME / EVENTS","WLED LIGHTS","LOG / HELP"};
        for(int i=0;i<names.length;i++){final int idx=i;Button b=action(names[i],page==i,()->{page=idx;render();});
            LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(57),1);p.setMargins(0,0,dp(9),0);nav.addView(b,p);}
        switch(page){case 0:pageScores();break;case 1:pageGame();break;case 2:pageLights();break;default:pageLog();break;}
        if(focusedTag!=null){View saved=root.findViewWithTag(focusedTag);if(saved!=null && saved.isFocusable()){saved.requestFocus();return;}}
        if(getCurrentFocus()==null && nav.getChildCount()>0)nav.getChildAt(page).requestFocus();
    }
    private void pageScores(){
        heading("LIVE NHL SCORES");subtitle("Direct NHL API  •  "+lastScoreAt+"  •  60-second updates");
        LinearLayout tools=row();buttons(tools,action("REFRESH NHL",true,()->refreshScores(true)));body.addView(tools);
        if(board==null){card("No games loaded yet","Press REFRESH NHL. Network errors appear in LOG / HELP.");return;}
        if(board.games.isEmpty()){card("No NHL games today",board.date+" — this is not a connection error.");return;}
        for(NhlApi.Game g:board.games){
            boolean selectedGame=selected!=null && selected.id==g.id;
            Button b=action(g.matchup()+"     "+g.score()+"      "+g.status(),selectedGame,()->chooseGame(g));
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(60));lp.setMargins(0,0,0,dp(8));body.addView(b,lp);
        }
    }
    private void pageGame(){
        heading("GAME / PLAY-BY-PLAY");
        if(selected==null){card("Select an NHL game","Go to NHL SCORES and press SELECT on a matchup to track it.");return;}
        card(selected.matchup()+"    "+selected.score(),selected.status()+"   •   Game ID "+selected.id);
        LinearLayout tools=row();buttons(tools,action("FETCH GAME EVENTS",true,()->checkPlays(false)),action("CHANGE GAME",false,()->{page=0;render();}));body.addView(tools);
        heading("Choose team for goal lighting");LinearLayout sides=row();
        buttons(sides,action("AWAY: "+selected.away,!favoriteHome,()->chooseSide(false)),action("HOME: "+selected.home,favoriteHome,()->chooseSide(true)));body.addView(sides);
        subtitle("TV stream delay: "+delaySec+" seconds (in 15s steps)");LinearLayout delays=row();
        buttons(delays,action("− 15s",false,()->setDelay(-15)),action("+ 15s",false,()->setDelay(15)));body.addView(delays);
        card("Live reaction status",armed?"ARMED • unique new NHL goal events only • starts with baseline":armPending?"Checking NHL goal baseline and WLED before arming...":"DISARMED (default after launch/restart)");
        LinearLayout armTools=row();
        buttons(armTools,action(armed?"DISARM GOALS":armPending?"ARMING…":"ARM LIVE GOALS",armed,()->{if(armed||armPending)disarm("User pressed disarm");else arm();render();}));body.addView(armTools);
        subtitle("Arming fetches a play-by-play baseline first. No replayed/old goals. One WLED target, static color, 8-second restore; no strobe.");
        if(!recentPlays.isEmpty()){
            heading("Recent NHL events");int start=Math.max(0,recentPlays.size()-12);
            for(int i=recentPlays.size()-1;i>=start;i--){NhlApi.Play p=recentPlays.get(i);
                subtitle("#"+p.eventId+"  "+p.description+(p.goal()?" • GOAL team ID "+p.ownerId:""));}
        }
    }
    private void pageLights(){
        heading("LOCAL WLED TEST");subtitle("Direct LAN HTTP  •  Fire TV and WLED must use the same reachable network");
        LinearLayout iprow=row();body.addView(iprow);
        ipEntry=new EditText(this);ipEntry.setText(wledIp);ipEntry.setSingleLine(true);ipEntry.setTextSize(22);
        ipEntry.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_VARIATION_URI);
        ipEntry.setTextColor(WHITE);ipEntry.setHintTextColor(MUTED);ipEntry.setHint("192.168.1.71");
        ipEntry.setBackground(shape(PANEL,0));ipEntry.setPadding(dp(15),dp(12),dp(15),dp(12));
        ipEntry.setTag("wled-ip");iprow.addView(ipEntry,new LinearLayout.LayoutParams(dp(310),dp(57)));
        Button connect=action("SAVE IP + CONNECT",true,()->connectWled());LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(-2,dp(57));cp.setMargins(dp(12),0,0,0);iprow.addView(connect,cp);
        body.addView(txt("Device: "+wledName+"  •  "+(wledConnected?"Connected":"Disconnected"),17,wledConnected?BLUE:MUTED,true));
        if(hasJournal()){card("RECOVERY REQUIRED","A previous preview hasn't been restored. New previews are blocked until restoration succeeds.");}
        LinearLayout opts=row();
        Button btnAway=action(selected==null?"TEST UTA BLUE  (8s)":"PREVIEW AWAY COLOR  (8s)",false,()->preview(false));
        Button btnHome=action(selected==null?"TEST BOS GOLD  (8s)":"PREVIEW HOME COLOR  (8s)",false,()->preview(true));
        buttons(opts,btnAway,btnHome);body.addView(opts);
        LinearLayout opts2=row();buttons(opts2,action("RESTORE PREVIOUS SCENE",true,()->restoreWled()),action("READ WLED STATE",false,()->readWled()));body.addView(opts2);
        if(selected==null)card("Standalone light test","No NHL game selected: UTA blue and BOS gold are available to verify WLED anytime.");
        else subtitle("Selected "+selected.away+" vs "+selected.home+" • segment 0 • brightness capped at 160 • previous state restored after 8s.");
        subtitle("Only RFC1918 private IPv4 addresses are accepted. No discovery or cloud relay. Never forward your WLED port to the internet.");
    }
    private void pageLog(){
        heading("CONNECTION / EVENT LOG");subtitle("The latest API errors, WLED commands and goal decisions appear here.");
        LinearLayout tools=row();buttons(tools,action("REFRESH SCORES",true,()->refreshScores(true)),action("CLEAR LOG",false,()->{lines.clear();log("Log cleared");render();}));body.addView(tools);
        for(int i=lines.size()-1;i>=0;i--)body.addView(txt(lines.get(i),16,MUTED,false));
        heading("Installation / safety");
        subtitle("- Use D-pad + SELECT; keyboard appears when editing WLED IP.\n- Any app exit disarms goal monitoring.\n- NHL API is public and best-effort.\n- A live NHL game is required for arming.\n- Goal events are monitored every 25 seconds after a safe baseline; delayed by your chosen TV delay.\n- Goal color is solid (no flashing), restored after 8 seconds.\n- Actual lights are controlled only when you preview or explicitly ARM.\n- If restore fails, open WLED tab and press RESTORE; recovery also retries on next launch.\n- If a new APK has a different debug signature, uninstall the previous test app before upgrading.");
    }
    private void log(String m){String stamp=DateFormat.getTimeInstance(DateFormat.MEDIUM,Locale.US).format(new Date());lines.add(stamp+"  "+m);if(lines.size()>70)lines.remove(0);android.util.Log.i("GlowDeckGameDay",m);}
    private void chooseGame(NhlApi.Game g){disarm("Game selection changed");selected=g;recentPlays.clear();goals.reset();page=1;log("Selected "+g.matchup()+" "+g.id);render();}
    private void chooseSide(boolean home){if(home!=favoriteHome){disarm("Favorite side changed");favoriteHome=home;}render();}
    private void setDelay(int delta){disarm("Delay changed");delaySec=Math.max(0,Math.min(90,delaySec+delta));prefs.edit().putInt("delaySec",delaySec).apply();render();}
    private interface Work<T>{T run()throws Exception;}
    private interface Result<T>{void ok(T x);}
    private <T>void job(String name,Work<T> fn,Result<T> done){io.execute(()->{
        try {T v=fn.run();ui.post(()->done.ok(v));}
        catch(Exception e){String error=e.getMessage()==null?e.toString():e.getMessage();ui.post(()->{log(name+": "+error);if(root!=null)render();});}
    });}
    private void refreshScores(boolean manual){if(scoreBusy)return;scoreBusy=true;lastScoresMs=System.currentTimeMillis();fetchStatus="fetching...";render();
        io.execute(()->{
            try{NhlApi.Board incoming=NhlApi.scores();ui.post(()->{
                scoreBusy=false;board=incoming;fetchStatus="online";lastScoreAt=incoming.date+" • "+DateFormat.getTimeInstance(DateFormat.SHORT).format(new Date());
                if(selected!=null){boolean found=false;for(NhlApi.Game g:incoming.games)if(g.id==selected.id){selected=g;found=true;break;}
                    if(!found && armed)disarm("Selected game absent from NHL scores");
                    if(armed&&selected!=null&&!selected.live())disarm("Selected game no longer live");}
                if(manual)log("NHL score API OK: "+incoming.games.size()+" games for "+incoming.date);
                render();
            });}catch(Exception e){String err=e.getMessage();ui.post(()->{scoreBusy=false;fetchStatus="error";log("NHL scores ERROR: "+err);if(armed)disarm("Score feed error");render();});}
        });
    }
    private String currentIp(){return ipEntry==null?wledIp:ipEntry.getText().toString().trim();}
    private void connectWled(){String ip=currentIp();if(!AddressValidator.isPrivateLanIpv4(ip)){log("Invalid WLED IP: private LAN IPv4 only");render();return;}
        disarm("WLED device changed");wledIp=ip;wledConnected=false;wledName="Connecting...";
        prefs.edit().putString("wledIp",wledIp).apply();render();
        job("WLED connection",()->WledApi.info(ip),info->{wledConnected=true;wledName=info.optString("name",ip)+" (v"+info.optString("ver","?")+")";
            log("WLED API OK at "+ip);render();});
    }
    private void readWled(){String ip=wledIp;job("WLED state",()->WledApi.state(ip),state->{log("WLED state: on="+state.optBoolean("on")+", brightness="+state.optInt("bri")+", segments="+(state.optJSONArray("seg")==null?0:state.optJSONArray("seg").length()));render();});}
    private boolean hasJournal(){return prefs!=null&&prefs.contains("restoreJson");}
    private void preview(boolean home){
        String team=selected==null?(home?"BOS":"UTA"):(home?selected.home:selected.away);
        startPreview(team,NhlApi.teamColor(team));
    }
    private void startPreview(String team,int rgb){
        if(hasJournal()||previewActive||restoreBusy){log("Preview blocked until prior state restored");render();return;}
        if(!wledConnected){log("Connect to WLED in LIGHTS first");render();return;}
        String ip=wledIp;previewActive=true;log("Preparing "+team+" preview; capturing WLED state first");render();
        io.execute(()->{
            try {
                JSONObject state=WledApi.state(ip);JSONObject saved=WledApi.snapshotRestore(state);
                if(!prefs.edit().putString("restoreJson",saved.toString()).putString("restoreIp",ip).commit())throw new Exception("Cannot save recovery journal");
                WledApi.send(ip,WledApi.previewColor(rgb,160));
                ui.post(()->{
                    log("WLED "+team+" color active for 8 seconds; prior state captured");
                    if(scheduledRestore!=null)ui.removeCallbacks(scheduledRestore);
                    scheduledRestore=()->restoreWled();ui.postDelayed(scheduledRestore,8000);render();
                });
            }catch(Exception e){String m=e.getMessage();ui.post(()->{previewActive=false;log("WLED preview ERROR: "+m);if(hasJournal())restoreWled();render();});}
        });
    }
    private void restoreWled(){
        if(!hasJournal()){previewActive=false;log("No outstanding restore journal");render();return;}
        if(restoreBusy)return;
        restoreBusy=true;
        if(scheduledRestore!=null){ui.removeCallbacks(scheduledRestore);scheduledRestore=null;}
        final String json=prefs.getString("restoreJson",""); final String ip=prefs.getString("restoreIp","");
        io.execute(()->{try{
            WledApi.send(ip,new JSONObject(json));
            boolean removed=prefs.edit().remove("restoreJson").remove("restoreIp").commit();
            ui.post(()->{restoreBusy=false;previewActive=false;
                log(removed?"WLED prior scene RESTORED":"WLED restored; journal clear failed, retry next launch");render();});
            }catch(Exception e){String msg=e.getMessage();ui.post(()->{restoreBusy=false;previewActive=false;
                log("RESTORE FAILED; state retained for recovery: "+msg);render();});}
        });
    }
    private void disarm(String reason){
        boolean was=armed||armPending;armed=false;armPending=false;goals.reset();
        if(pendingGoal!=null){ui.removeCallbacks(pendingGoal);pendingGoal=null;}
        if(was)log("DISARMED: "+reason);
        if(hasJournal()&&!restoreBusy)restoreWled();
    }
    private void arm(){
        if(selected==null||!selected.live()){log("Arm requires a selected LIVE NHL game");render();return;}
        if(!wledConnected || hasJournal()||previewActive){log("Arm requires connected WLED and no pending restore");render();return;}
        if(armPending)return;
        armPending=true;goals.reset();checkPlays(true);render();
    }
    private void checkPlays(boolean initial){
        if(pbpBusy || selected==null)return;
        pbpBusy=true;lastPbpMs=System.currentTimeMillis();final long gameId=selected.id;
        io.execute(()->{try{
            NhlApi.Pbp incoming=NhlApi.plays(gameId);
            ui.post(()->{
                pbpBusy=false;if(selected==null||selected.id!=gameId)return;
                recentPlays=incoming.plays;
                List<GoalTracker.Event> ev=new ArrayList<>();
                for(NhlApi.Play p:incoming.plays)if(p.eventId>0)ev.add(new GoalTracker.Event(p.eventId,p.ownerId,p.goal()));
                if(initial){if(!armPending)return;
                    if(incoming.awayId==0||incoming.homeId==0){armPending=false;log("Cannot arm: NHL team IDs missing");render();return;}
                    goals.update(ev);armed=true;armPending=false;
                    log("ARMED after PBP baseline ("+incoming.plays.size()+" plays); waiting for a NEW favorite-team goal");
                }else if(armed){
                    if(incoming.awayId==0||incoming.homeId==0){disarm("NHL owner team IDs unavailable");render();return;}
                    int owner=goals.update(ev),favoriteId=favoriteHome?incoming.homeId:incoming.awayId;
                    if(owner>0){
                        if(owner==favoriteId)queueGoal(selected,gameId);
                        else log("Opponent goal # detected; no selected-team reaction");
                    }
                }else goals.update(ev); // read-only fetch creates no armed event
                if(!initial)log("NHL PBP OK: "+incoming.plays.size()+" plays; "+incoming.homeId+" / "+incoming.awayId);
                render();
            });
        }catch(Exception e){String m=e.getMessage();ui.post(()->{
            pbpBusy=false;armPending=false;if(armed)disarm("NHL PBP request error");
            log("NHL PBP error: "+m);render();});}
        });
    }
    private void queueGoal(NhlApi.Game g,long expectedGame){
        long now=System.currentTimeMillis();
        if(!armed||hasJournal()||previewActive||pendingGoal!=null||now-lastEffectMs<20000){log("Goal ignored: busy / recovery / cooldown");return;}
        String team=favoriteHome?g.home:g.away;int color=NhlApi.teamColor(team);
        log("NEW NHL "+team+" goal! Scheduled WLED in "+delaySec+" seconds");
        pendingGoal=()->{
            pendingGoal=null;
            if(armed&&selected!=null&&selected.id==expectedGame&&selected.live()&&!hasJournal()){
                lastEffectMs=System.currentTimeMillis();startPreview(team,color);
            }else {log("Scheduled goal dropped: no longer armed/live");render();}
        };
        ui.postDelayed(pendingGoal,delaySec*1000L);
    }
}
