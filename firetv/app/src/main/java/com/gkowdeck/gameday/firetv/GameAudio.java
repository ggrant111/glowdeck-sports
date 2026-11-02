package com.gkowdeck.gameday.firetv;

import android.content.Context;
import android.content.SharedPreferences;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

/** Isolated audio worker. Never invoked on the NHL/WLED polling executor. */
final class GameAudio {
    private final Context context;
    private final SharedPreferences profile;
    private final AtomicBoolean stop=new AtomicBoolean();
    private volatile Thread playing;
    private volatile AudioTrack track;
    GameAudio(Context context){
        this.context=context.getApplicationContext();
        profile=this.context.getSharedPreferences("gameday-audio",Context.MODE_PRIVATE);
    }
    static File audioDirectory(Context c){
        File base=c.getExternalFilesDir(null);
        return new File(base==null?c.getFilesDir():base,"gameday-audio");
    }
    boolean isPlaying(){return playing!=null && playing.isAlive();}
    void stop(){
        stop.set(true);
        AudioTrack existing=track;
        if(existing!=null){try{existing.pause();existing.flush();}catch(Exception ignore){}}
    }
    boolean play(String team,boolean preview){
        if(!preview && !profile.getBoolean("enabled",false))return false;
        if(team==null||!team.matches("[A-Z]{3}"))return false;
        if(isPlaying())return false; // don't overlap, goal cooldown already applies
        int kind=AudioProfile.kind(profile.getInt("kind."+team,AudioProfile.HORN));
        if(kind==AudioProfile.OFF)return false;
        int volume=AudioProfile.volume(profile.getInt("volume",50));
        if(volume==0)return false;
        String clip=profile.getString("clip."+team,team);
        stop.set(false);
        Thread t=new Thread(()->{
            try {
                if(kind==AudioProfile.CUSTOM){
                    try{wav(clip,volume);}
                    catch(Exception ex){
                        Log.w("GlowDeckAudio","Custom WAV unavailable, using built-in horn: "+ex.getMessage());
                        if(!stop.get())synth(AudioProfile.HORN,volume);
                    }
                }else synth(kind,volume);
            }catch(Exception ex){Log.e("GlowDeckAudio","Audio playback failed",ex);}
            finally{playing=null;}
        },"GlowDeckAudio");
        playing=t;t.setDaemon(true);t.start();
        return true;
    }
    private AudioTrack openTrack(){
        int buffer=Math.max(2048,AudioTrack.getMinBufferSize(16000,
            AudioFormat.CHANNEL_OUT_MONO,AudioFormat.ENCODING_PCM_16BIT));
        AudioAttributes att=new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
        AudioFormat fmt=new AudioFormat.Builder().setSampleRate(16000)
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO).build();
        AudioTrack result=new AudioTrack(att,fmt,buffer*2,AudioTrack.MODE_STREAM,
            android.media.AudioManager.AUDIO_SESSION_ID_GENERATE);
        if(result.getState()!=AudioTrack.STATE_INITIALIZED)throw new IllegalStateException("AudioTrack unavailable");
        track=result;result.play();return result;
    }
    private void close(AudioTrack t){
        try{t.stop();}catch(Exception ignore){}
        try{t.release();}catch(Exception ignore){}
        if(track==t)track=null;
    }
    private void synth(int kind,int volume){
        AudioTrack t=openTrack();
        try{
            int frames=(kind==AudioProfile.SIREN?1700:kind==AudioProfile.DOUBLE?2100:2400)*16;
            short[] samples=new short[256];
            for(int i=0;i<frames&&!stop.get();){
                int count=Math.min(samples.length,frames-i);
                for(int j=0;j<count;j++,i++){
                    double sec=(double)i/16000.0;
                    double freq=kind==AudioProfile.HORN?196:
                      kind==AudioProfile.DOUBLE?(sec<1?247:294):300+140*Math.sin(sec*9);
                    double envelope=Math.min(1,sec/0.12)*Math.min(1,(frames-i)/3200.0);
                    double value=(.65*Math.sin(2*Math.PI*freq*sec)+.25*Math.sin(4*Math.PI*freq*sec));
                    samples[j]=(short)(9000*value*envelope*volume/100.0);
                }
                int wrote=t.write(samples,0,count);
                if(wrote<0)break;
            }
        }finally{close(t);}
    }
    private void wav(String fileName,int volume) throws Exception {
        if(!AudioProfile.validName(fileName))throw new IOException("Invalid custom file name");
        File file=new File(audioDirectory(context),fileName+".wav");
        try(FileInputStream in=new FileInputStream(file)){
            byte[] h=new byte[44];
            if(in.read(h)!=44)throw new IOException("WAV too small");
            int remaining=AudioProfile.wavBytes(h,file.length());
            if(remaining<0)throw new IOException("Expected mono PCM 16-kHz/16-bit WAV <=10 sec");
            AudioTrack t=openTrack();
            try{
                byte[] bytes=new byte[2048];short[] shorts=new short[1024];
                while(remaining>0&&!stop.get()){
                    int n=Math.min(bytes.length,remaining);int got=0;
                    while(got<n){
                        int d=in.read(bytes,got,n-got);if(d<0)throw new IOException("Truncated WAV");
                        got+=d;
                    }
                    for(int i=0;i<n/2;i++){
                        short v=(short)((bytes[2*i]&255)|((bytes[2*i+1]&255)<<8));
                        shorts[i]=(short)(v*volume/100);
                    }
                    if(t.write(shorts,0,n/2)<0)throw new IOException("Audio output failed");
                    remaining-=n;
                }
            }finally{close(t);}
        }
    }
}
