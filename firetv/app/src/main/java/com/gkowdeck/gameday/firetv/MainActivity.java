package com.gkowdeck.gameday.firetv;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Handler;
import android.net.Uri;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.InputStream;
import android.os.Looper;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
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

/** Leanback TV UI. Foreground service handles monitoring independently of the Activity. */
public final class MainActivity extends Activity {
    private static final int BG=Color.rgb(9,19,23), PANEL=Color.rgb(26,46,54), TEAL=Color.rgb(49,131,122);
    private static final int WHITE=Color.rgb(241,249,250), MUTED=Color.rgb(169,192,199), BLUE=Color.rgb(108,172,228), GOLD=Color.rgb(255,184,28);
    private final ExecutorService io=Executors.newSingleThreadExecutor();
    private final Handler ui=new Handler(Looper.getMainLooper());
    private LinearLayout root,nav,body; private TextView status,notice;private EditText ipEntry;
    private SharedPreferences prefs, audioPrefs;
    private GameAudio gameAudio;
    private LanSoundServer lanSounds;
    private String importingTeam="UTA";
    private static final int IMPORT_WAV=910;
    private NhlApi.Board board; private NhlApi.Game selected;
    private final List<String> lines=new ArrayList<>();
    private List<NhlApi.Play> recentPlays=new ArrayList<>();
    private int page=0,delaySec=0,celebrationMode=CelebrationSelection.AWAY;private String serviceState="";
    private boolean wledConnected=false,scoreBusy=false,pbpBusy=false,resumed=false,previewActive=false,restoreBusy=false;
    private String wledIp="192.168.1.71",wledName="Not connected",fetchStatus="Not checked",lastScoreAt="Never";
    private long lastScoresMs=0,lastPbpMs=0;
    private Runnable scheduledRestore;
    private final DdpStream previewStream=new DdpStream();
    private final Runnable tick=new Runnable(){public void run(){
        if(!resumed)return;
        long now=System.currentTimeMillis();
        if(now-lastScoresMs>60000)refreshScores(false);
        String current=monitorStatus();
        if(!current.equals(serviceState)){serviceState=current;render();}
        ui.postDelayed(this,1000);
    }};

    @Override public void onCreate(Bundle state){
        super.onCreate(state);
        if(Build.VERSION.SDK_INT>=33 && checkSelfPermission("android.permission.POST_NOTIFICATIONS") != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 1001);
        }
        prefs=getSharedPreferences("gameday-lab",MODE_PRIVATE);
        audioPrefs=getSharedPreferences("gameday-audio",MODE_PRIVATE);
        // Privacy/safety: a freshly launched Activity never silently re-enables
        // background sound, even if an earlier session had sound enabled.
        audioPrefs.edit().putBoolean("enabled",false).commit();
        gameAudio=new GameAudio(this);
        lanSounds=new LanSoundServer(this,gameAudio,()->ui.post(()->{
            if(resumed && page==3)render();
        }));
        wledIp=prefs.getString("wledIp","192.168.1.71");
        delaySec=prefs.getInt("delaySec",0);
        log("GameDay Fire TV v0.4.1 started. Local iPhone audio management is available.");
        buildShell();render();
        if(hasJournal() && !GameDayService.isRunning()){log("Found interrupted WLED preview; attempting restore before new commands.");restoreWled();}
    }
    @Override protected void onResume(){
        super.onResume();resumed=true;
        ui.removeCallbacks(tick);serviceState="";ui.post(tick);
        if(page==3 && lanSounds!=null)lanSounds.start();
    }
    @Override protected void onStop(){
        resumed=false;
        ui.removeCallbacks(tick);
        if(lanSounds!=null)lanSounds.stop(); // Never expose uploads while TV UI is hidden
        stopPreviewOnExit();
        /* Foreground NHL/WLED service keeps monitoring. */
        super.onStop();
    }
    @Override protected void onDestroy(){
        stopPreviewOnExit();
        previewStream.close();
        ui.removeCallbacks(tick);
        if(scheduledRestore!=null)ui.removeCallbacks(scheduledRestore);
        if(lanSounds!=null)lanSounds.stop();
        if(gameAudio!=null)gameAudio.stop();
        io.shutdown();super.onDestroy();
    }

    private void stopPreviewOnExit(){
        if(!previewActive || GameDayService.isRunning() || io.isShutdown())return;
        if(scheduledRestore!=null)ui.removeCallbacks(scheduledRestore);
        io.execute(()->{
            previewStream.close();
            if(!prefs.contains("restoreJson"))return;
            try{
                WledApi.send(prefs.getString("restoreIp",""),new JSONObject(prefs.getString("restoreJson","")).put("live",false));
                prefs.edit().remove("restoreJson").remove("restoreIp").commit();
            }catch(Exception ex){android.util.Log.w("GameDay","Preview exit restore pending",ex);}
            ui.post(()->{previewActive=false;});
        });
    }
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
        if(lanSounds!=null){
            if(resumed&&page==3)lanSounds.start();
            else lanSounds.stop();
        }
        String focusedTag=null;View focus=getCurrentFocus();
        if(focus!=null&&focus.getTag() instanceof String)focusedTag=(String)focus.getTag();
        nav.removeAllViews();body.removeAllViews();
        status.setText("NHL: "+fetchStatus+"   |   MONITOR: "+monitorStatus());
        String[] names={"NHL SCORES","GAME / EVENTS","WLED LIGHTS","SOUND","LOG / HELP"};
        for(int i=0;i<names.length;i++){final int idx=i;Button b=action(names[i],page==i,()->{page=idx;render();});
            LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(57),1);p.setMargins(0,0,dp(9),0);nav.addView(b,p);}
        switch(page){case 0:pageScores();break;case 1:pageGame();break;case 2:pageLights();break;case 3:pageAudio();break;default:pageLog();break;}
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
        if(selected==null){
            if(GameDayService.isRunning()){
                card("Monitoring in background",monitorStatus()+" • "+monitorDetail());
                LinearLayout stop=row();buttons(stop,action("STOP BACKGROUND MONITOR",true,()->disarm("Stop requested")));body.addView(stop);
            } else card("Select an NHL game","Go to NHL SCORES and press SELECT on a matchup to track it.");
            return;
        }
        card(selected.matchup()+"    "+selected.score(),selected.status()+"   •   Game ID "+selected.id);
        LinearLayout tools=row();buttons(tools,action("FETCH GAME EVENTS",true,()->checkPlays(false)),action("CHANGE GAME",false,()->{page=0;render();}));body.addView(tools);
        heading("Celebrate goals for");
        subtitle("Choose whose goals trigger lights; BOTH uses the actual scoring team's color.");
        LinearLayout sides=row();
        buttons(sides,
            action("AWAY: "+selected.away,celebrationMode==CelebrationSelection.AWAY,()->chooseCelebrations(CelebrationSelection.AWAY)),
            action("HOME: "+selected.home,celebrationMode==CelebrationSelection.HOME,()->chooseCelebrations(CelebrationSelection.HOME)),
            action("BOTH TEAMS",celebrationMode==CelebrationSelection.BOTH,()->chooseCelebrations(CelebrationSelection.BOTH)));
        body.addView(sides);
        subtitle("TV stream delay: "+delaySec+" seconds (in 15s steps)");LinearLayout delays=row();
        buttons(delays,action("− 15s",false,()->setDelay(-15)),action("+ 15s",false,()->setDelay(15)));body.addView(delays);
        card("Background monitoring",monitorStatus()+"  •  "+monitorDetail());
        LinearLayout armTools=row();
        buttons(armTools,action(GameDayService.isRunning()?"STOP BACKGROUND MONITOR":"ARM IN BACKGROUND",GameDayService.isRunning(),()->{if(GameDayService.isRunning())disarm("Stop requested");else arm();render();}));body.addView(armTools);
        subtitle("Start here, then press HOME and watch YouTube TV/ESPN/Prime. Monitor continues as a foreground service until STOP, game final, or system stop. Baselines ignore older goals.");
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
        if(hasJournal() && !GameDayService.isRunning()){card("RECOVERY REQUIRED","A previous preview hasn't been restored. New previews are blocked until restoration succeeds.");}
        LinearLayout opts=row();
        boolean ddp=prefs.getBoolean("ddp",false);
        subtitle("DDP realtime: "+(ddp?"ON":"OFF")+" • whole device • 20fps • 1–600 LEDs • eight-second effects");
        LinearLayout streaming=row();buttons(streaming,action(ddp?"USE SOLID COLORS":"ENABLE DDP WHOLE STRIP",ddp,()->{
            if(denyChangeWhileMonitoring()||hasJournal()||previewActive)return;
            prefs.edit().putBoolean("ddp",!ddp).apply();render();}));body.addView(streaming);
        if(ddp){
            subtitle("Enable WLED realtime receive; disable Main segment only. Do not run another sender on this strip.");
            for(int index=0;index<EventEffects.NAMES.length;index++){
                final int kind=index;boolean enabled=(prefs.getInt("eventMask",127)&(1<<kind))!=0;
                LinearLayout effect=row();buttons(effect,
                    action(EventEffects.NAMES[kind]+(enabled?" ON":" OFF"),enabled,()->{
                        if(denyChangeWhileMonitoring()||hasJournal())return;
                        prefs.edit().putInt("eventMask",prefs.getInt("eventMask",127)^(1<<kind)).apply();render();}),
                    action("PREVIEW AWAY",false,()->previewEffect(kind,false)),
                    action("PREVIEW HOME",false,()->previewEffect(kind,true)));body.addView(effect);
            }
        }
        Button btnAway=action(selected==null?"TEST UTA BLUE  (8s)":"PREVIEW AWAY COLOR  (8s)",false,()->preview(false));
        Button btnHome=action(selected==null?"TEST BOS GOLD  (8s)":"PREVIEW HOME COLOR  (8s)",false,()->preview(true));
        buttons(opts,btnAway,btnHome);body.addView(opts);
        LinearLayout opts2=row();buttons(opts2,action("RESTORE PREVIOUS SCENE",true,()->restoreWled()),action("READ WLED STATE",false,()->readWled()));body.addView(opts2);
        if(selected==null)card("Standalone light test","No NHL game selected: UTA blue and BOS gold are available to verify WLED anytime.");
        else subtitle("Selected "+selected.away+" vs "+selected.home+" • segment 0 • brightness capped at 160 • previous state restored after 8s.");
        subtitle("Only RFC1918 private IPv4 addresses are accepted. No discovery or cloud relay. Never forward your WLED port to the internet.");
    }

    private void pageAudio(){
        heading("GAME DAY SOUNDS");
        subtitle("Three original built-in horns or custom local WAV • independent of WLED");
        if(lanSounds!=null){
            if(lanSounds.ready()){
                card("MANAGE SOUNDS FROM YOUR iPHONE",
                    "Open Safari on the same Wi-Fi: "+lanSounds.url());
                TextView access=txt("PAIRING CODE:  "+lanSounds.pairingCode(),30,GOLD,true);
                body.addView(access);
                subtitle("Enter the code in Safari. Keep this SOUND screen open while uploading. Server closes when you switch apps or leave the page.");
            }else if(!lanSounds.error().isEmpty()){
                card("LOCAL SOUND SERVER ERROR",lanSounds.error());
                LinearLayout again=row();
                buttons(again,action("RESTART LOCAL MANAGER",true,()->{
                    lanSounds.restart();render();
                }));body.addView(again);
            }else{
                card("Starting local sound manager",
                    "Open this page from Safari after the Fire TV IP address and pairing code appear.");
            }
            subtitle("Private home network only • Port 8788 • No cloud • No port forwarding");
        }
        final boolean enabled=audioPrefs.getBoolean("enabled",false);
        final int volume=AudioProfile.volume(audioPrefs.getInt("volume",50));
        LinearLayout master=row();
        buttons(master,
          action(enabled?"SOUND ON • TAP TO MUTE":"SOUND OFF • TAP TO ENABLE",enabled,()->
            {audioPrefs.edit().putBoolean("enabled",!enabled).apply();if(enabled)gameAudio.stop();render();}),
          action("VOLUME −10",false,()->changeAudioVolume(-10)),
          action("VOLUME +10",false,()->changeAudioVolume(10)));
        body.addView(master);
        subtitle("Volume: "+volume+"% (independent of TV video volume; stream mixing varies by Fire OS)");
        String away=selected==null?"UTA":selected.away;
        String home=selected==null?"BOS":selected.home;
        soundTeamRow(away,"AWAY");
        soundTeamRow(home,"HOME");
        card("Custom sound format","WAV PCM mono, 16 kHz, 16-bit, 1–10 seconds, <=320 KB audio. Choose IMPORT WAV or copy via ADB to app's gameday-audio folder. If a custom file is missing, a built-in horn plays instead.");
        subtitle("IMPORTANT: Background sounds are OFF until enabled here. TV/YouTube audio focus may suppress or mix them; no interruption of WLED restore.");
    }
    private void soundTeamRow(String team,String side){
        int current=AudioProfile.kind(audioPrefs.getInt("kind."+team,AudioProfile.HORN));
        String file=audioPrefs.getString("clip."+team,team);
        heading(side+" · "+team);
        subtitle("Sound: "+AudioProfile.name(current)+(current==AudioProfile.CUSTOM?" ("+file+".wav)":""));
        LinearLayout row=row();
        buttons(row,
            action("NEXT SOUND",false,()->{audioPrefs.edit().putInt("kind."+team,(current+1)%5).apply();render();}),
            action("▶ PREVIEW",true,()->{if(gameAudio.play(team,true))log("Sound preview "+team);
              else log("Audio preview unavailable / busy / OFF");render();}),
            action("IMPORT WAV",false,()->importWav(team)));
        body.addView(row);
    }
    private void changeAudioVolume(int delta){
        int n=AudioProfile.volume(audioPrefs.getInt("volume",50)+delta);
        audioPrefs.edit().putInt("volume",n).apply();render();
    }
    private void importWav(String team){
        if(GameDayService.isRunning()){
            log("Stop monitoring before importing audio to avoid interrupting a goal");render();return;
        }
        importingTeam=team;
        Intent pick=new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        pick.setType("*/*");
        pick.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try{startActivityForResult(pick,IMPORT_WAV);}
        catch(Exception ex){log("File picker not installed on this Fire TV. Use ADB copy instructions: "+ex.getMessage());render();}
    }
    @Override protected void onActivityResult(int code,int result,Intent data){
        super.onActivityResult(code,result,data);
        if(code!=IMPORT_WAV || result!=RESULT_OK || data==null || data.getData()==null)return;
        Uri uri=data.getData();String team=importingTeam;
        io.execute(()->{
            File folder=GameAudio.audioDirectory(this);
            File temp=new File(folder,team+".tmp"),dst=new File(folder,team+".wav");
            try{
                if(!folder.exists()&&!folder.mkdirs())throw new Exception("Cannot create audio folder");
                long len=0;
                try(InputStream in=getContentResolver().openInputStream(uri);
                    FileOutputStream out=new FileOutputStream(temp)){
                    if(in==null)throw new Exception("Cannot read selected WAV");
                    byte[] buf=new byte[2048];int n;
                    while((n=in.read(buf))!=-1){
                        len+=n;if(len>320044)throw new Exception("WAV file exceeds 10-second limit");
                        out.write(buf,0,n);
                    }
                }
                byte[] h=new byte[44];
                try(FileInputStream in=new FileInputStream(temp)){
                    if(in.read(h)!=44 || AudioProfile.wavBytes(h,temp.length())<0)
                        throw new Exception("Select canonical mono PCM 16kHz 16-bit WAV");
                }
                if(dst.exists()&&!dst.delete())throw new Exception("Cannot replace existing WAV");
                if(!temp.renameTo(dst))throw new Exception("Cannot store WAV");
                audioPrefs.edit().putString("clip."+team,team).putInt("kind."+team,AudioProfile.CUSTOM).apply();
                ui.post(()->{log("Imported "+team+".wav successfully");render();});
            }catch(Exception ex){
                temp.delete();String error=ex.getMessage();
                ui.post(()->{log("WAV import error: "+error);render();});
            }
        });
    }
    private void pageLog(){
        heading("CONNECTION / EVENT LOG");subtitle("The NHL/API diagnostic log is visible here. Background monitor state: "+monitorStatus()+". For background detail use adb logcat -s GlowDeckMonitor.");
        LinearLayout tools=row();buttons(tools,action("REFRESH SCORES",true,()->refreshScores(true)),action("CLEAR LOG",false,()->{lines.clear();log("Log cleared");render();}));body.addView(tools);
        for(int i=lines.size()-1;i>=0;i--)body.addView(txt(lines.get(i),16,MUTED,false));
        heading("Installation / safety");
        subtitle("- Select LIVE NHL game and connected WLED, then ARM IN BACKGROUND.\n- Press HOME to watch a streaming app; foreground service keeps polling.\n- The ongoing notification has a STOP monitoring action.\n- Return to GameDay and press STOP to end. Reboot/force-stop does not re-arm.\n- NHL play-by-play polling: 25s; feed errors pause then re-baseline.\n- Choose AWAY, HOME or BOTH celebrations per selected game. Both uses the scoring team color for 8s and restores WLED.\n- If restore fails, stop and open WLED tab to retry; never use safety-critical lights.\n- Fire OS memory/sleep restrictions may stop the service; verify on your own Fire TV.");
    }
    private void log(String m){String stamp=DateFormat.getTimeInstance(DateFormat.MEDIUM,Locale.US).format(new Date());lines.add(stamp+"  "+m);if(lines.size()>70)lines.remove(0);android.util.Log.i("GlowDeckGameDay",m);}
    private void chooseGame(NhlApi.Game g){
        if(denyChangeWhileMonitoring())return;
        selected=g;
        celebrationMode=CelebrationSelection.normalize(prefs.getInt("celebrationMode."+g.id,CelebrationSelection.AWAY));
        recentPlays.clear();page=1;log("Selected "+g.matchup()+" "+g.id+" • celebrate "+CelebrationSelection.label(celebrationMode,g.away,g.home));render();
    }
    private void chooseCelebrations(int mode){
        if(denyChangeWhileMonitoring() || selected==null)return;
        celebrationMode=CelebrationSelection.normalize(mode);
        prefs.edit().putInt("celebrationMode."+selected.id,celebrationMode).apply();
        log("Celebrate "+CelebrationSelection.label(celebrationMode,selected.away,selected.home)+" goals");render();
    }
    private void setDelay(int delta){if(denyChangeWhileMonitoring())return;delaySec=Math.max(0,Math.min(90,delaySec+delta));prefs.edit().putInt("delaySec",delaySec).apply();render();}
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
                if(selected!=null){for(NhlApi.Game g:incoming.games)if(g.id==selected.id){selected=g;break;}}
                if(manual)log("NHL score API OK: "+incoming.games.size()+" games for "+incoming.date);
                render();
            });}catch(Exception e){String err=e.getMessage();ui.post(()->{scoreBusy=false;fetchStatus="error";log("NHL scores ERROR: "+err);render();});}
        });
    }
    private String currentIp(){return ipEntry==null?wledIp:ipEntry.getText().toString().trim();}
    private void connectWled(){if(denyChangeWhileMonitoring())return;String ip=currentIp();if(!AddressValidator.isPrivateLanIpv4(ip)){log("Invalid WLED IP: private LAN IPv4 only");render();return;}
        wledIp=ip;wledConnected=false;wledName="Connecting...";
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
    private void previewEffect(int kind,boolean home){
        String team=selected==null?(home?"BOS":"UTA"):(home?selected.home:selected.away);
        startPreview(team,NhlApi.teamColor(team),kind);
    }
    private void startPreview(String team,int rgb){startPreview(team,rgb,EventEffects.GOAL);}
    private void startPreview(String team,int rgb,int kind){
        if(denyChangeWhileMonitoring())return;
        if(hasJournal()||previewActive||restoreBusy){log("Preview blocked until prior state restored");render();return;}
        if(!wledConnected){log("Connect to WLED in LIGHTS first");render();return;}
        final boolean ddp=prefs.getBoolean("ddp",false);
        String ip=wledIp;previewActive=true;log("Preparing "+team+" preview; capturing WLED state first");render();
        io.execute(()->{
            try {
                int pixels=ddp?DdpScene.count(ip):0;
                JSONObject state=WledApi.state(ip);JSONObject saved=WledApi.snapshotRestore(state);
                if(!prefs.edit().putString("restoreJson",saved.toString()).putString("restoreIp",ip).commit())throw new Exception("Cannot save recovery journal");
                if(ddp)DdpScene.start(previewStream,ip,pixels,kind,rgb);
                else WledApi.send(ip,WledApi.previewColor(rgb,160));
                ui.post(()->{
                    log("WLED "+team+" color active for 8 seconds; prior state captured");
                    if(scheduledRestore!=null)ui.removeCallbacks(scheduledRestore);
                    scheduledRestore=()->restoreWled();ui.postDelayed(scheduledRestore,8000);render();
                });
            }catch(Exception e){String m=e.getMessage();ui.post(()->{previewActive=false;log("WLED preview ERROR: "+m);if(hasJournal())restoreWled();render();});}
        });
    }
    private void restoreWled(){
        if(GameDayService.isRunning()){log("Monitoring active. STOP first, then restore if needed.");render();return;}
        if(!hasJournal()){previewActive=false;log("No outstanding restore journal");render();return;}
        if(restoreBusy)return;
        restoreBusy=true;
        previewStream.close();
        if(scheduledRestore!=null){ui.removeCallbacks(scheduledRestore);scheduledRestore=null;}
        final String json=prefs.getString("restoreJson",""); final String ip=prefs.getString("restoreIp","");
        io.execute(()->{try{
            previewStream.close();
            WledApi.send(ip,new JSONObject(json).put("live",false));
            boolean removed=prefs.edit().remove("restoreJson").remove("restoreIp").commit();
            ui.post(()->{restoreBusy=false;previewActive=false;
                log(removed?"WLED prior scene RESTORED":"WLED restored; journal clear failed, retry next launch");render();});
            }catch(Exception e){String msg=e.getMessage();ui.post(()->{restoreBusy=false;previewActive=false;
                log("RESTORE FAILED; state retained for recovery: "+msg);render();});}
        });
    }
    private String monitorStatus(){
        if(GameDayService.isStopping())return "STOPPING";
        if(!GameDayService.isRunning())return "STOPPED";
        return getSharedPreferences("gameday-monitor",MODE_PRIVATE).getString("mode","STARTING");
    }
    private String monitorDetail(){
        return getSharedPreferences("gameday-monitor",MODE_PRIVATE).getString("detail","No active session");
    }
    private boolean denyChangeWhileMonitoring(){
        if(!GameDayService.isRunning() && !GameDayService.isStopping())return false;
        log("Stop background monitoring before changing a game, delay, WLED IP or manual scene");render();return true;
    }
    private void disarm(String reason){
        if(!GameDayService.isRunning())return;
        log("Stopping foreground monitor: "+reason);
        Intent i=new Intent(this,GameDayService.class).setAction(GameDayService.STOP);
        startService(i);
        render();
    }
    private void arm(){
        if(GameDayService.isRunning()||GameDayService.isStopping())return;
        if(selected==null||!selected.live()){log("Select a LIVE NHL game before monitoring");render();return;}
        if(!wledConnected||!AddressValidator.isPrivateLanIpv4(wledIp)||hasJournal()||previewActive){
            log("Connect a valid WLED device; finish any pending scene restore first");render();return;
        }
        Intent i=new Intent(this,GameDayService.class).setAction(GameDayService.START)
                .putExtra("gameId",selected.id).putExtra("away",selected.away)
                .putExtra("home",selected.home).putExtra("wledIp",wledIp)
                .putExtra("celebrationMode",celebrationMode).putExtra("delaySec",delaySec);
        try {
            if(Build.VERSION.SDK_INT>=26)startForegroundService(i);else startService(i);
            log("Background goal monitoring requested. Switch to your streaming app after ARMED appears.");
        }catch(Exception e){log("Unable to start foreground monitor: "+e.getMessage());}
        render();
    }
    private void checkPlays(boolean ignored){
        if(pbpBusy||selected==null)return;
        pbpBusy=true;lastPbpMs=System.currentTimeMillis();long id=selected.id;
        io.execute(()->{
            try{
                NhlApi.Pbp incoming=NhlApi.plays(id);
                ui.post(()->{pbpBusy=false;if(selected!=null&&selected.id==id){
                    recentPlays=incoming.plays;
                    log("NHL PBP OK: "+incoming.plays.size()+" plays • read-only");render();
                }});
            }catch(Exception e){String err=e.getMessage();ui.post(()->{pbpBusy=false;log("PBP: "+err);render();});}
        });
    }
}
