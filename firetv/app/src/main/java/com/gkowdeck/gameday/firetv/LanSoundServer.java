package com.gkowdeck.gameday.firetv;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.NetworkInterface;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

/**
 * Short-lived, Fire-TV-hosted web sound manager. It exists ONLY while the SOUND
 * screen is open. Requires an eight-digit pairing code displayed on the TV.
 * Requests are same-origin and accepted only from RFC1918 IPv4 LAN clients.
 *
 * IMPORTANT: Plain HTTP (not TLS). Use only on trusted home Wi-Fi. Never port
 * forward port 8788. No third-party libraries, cloud, or computer server.
 */
final class LanSoundServer {
    private static final String TAG="GlowDeckSoundLAN";
    private static final int MAX_JSON=2048;
    private final Context context;
    private final SharedPreferences prefs;
    private final GameAudio audio;
    private final Runnable changed;
    private final SecureRandom random=new SecureRandom();

    private volatile boolean wanted=false,ready=false;
    private volatile long generation=0;
    private volatile String code="",error="",address="";
    private volatile ServerSocket listener;
    private volatile Socket active;
    private volatile Thread worker;

    LanSoundServer(Context context,GameAudio audio,Runnable onStatusChange){
        this.context=context.getApplicationContext();
        this.prefs=this.context.getSharedPreferences("gameday-audio",Context.MODE_PRIVATE);
        this.audio=audio;
        this.changed=onStatusChange;
    }

    synchronized void start(){
        if(wanted||!error.isEmpty())return;
        wanted=true;
        final long epoch=++generation;
        ready=false;
        error="";
        code=String.format(Locale.US,"%08d",random.nextInt(100000000));
        address=localIPv4();
        worker=new Thread(()->listen(epoch),"GlowDeckSoundLAN");
        worker.setDaemon(true);
        worker.start();
    }

    synchronized void stop(){
        wanted=false;
        ++generation;
        ready=false;
        code=""; // invalidate session immediately on leaving SOUND screen
        closeQuietly(active);
        closeQuietly(listener);
        active=null;
        listener=null;
    }

    synchronized void restart(){stop();error="";start();}

    boolean ready(){return ready;}
    String pairingCode(){return code;}
    String url(){return address.isEmpty()?"":"http://"+address+":"+LanSoundProtocol.PORT+"/";}
    String error(){return error;}

    private void fireChanged(){
        if(changed!=null)changed.run();
    }

    private void listen(long epoch){
        try(ServerSocket server=new ServerSocket()){
            server.setReuseAddress(true);
            server.bind(new InetSocketAddress(LanSoundProtocol.PORT),4);
            if(!wanted||epoch!=generation)return;
            listener=server;
            ready=true;
            fireChanged();
            Log.i(TAG,"LAN audio manager ready on "+url()+" (pairing code displayed on Fire TV)");
            while(wanted&&epoch==generation){
                try(Socket client=server.accept()){
                    active=client;
                    client.setSoTimeout(15000);
                    InetAddress origin=client.getInetAddress();
                    if(!(origin instanceof Inet4Address) ||
                       !AddressValidator.isPrivateLanIpv4(origin.getHostAddress())){
                        send(client.getOutputStream(),403,"application/json",
                             jsonError("Private home network only"));
                    } else {
                        respond(client);
                    }
                }catch(IOException ex){
                    if(wanted)Log.w(TAG,"LAN sound request: "+ex.getMessage());
                }catch(Exception ex){
                    if(wanted)Log.e(TAG,"LAN sound request failed",ex);
                }finally{
                    if(active!=null&&active.isClosed())active=null;
                }
            }
        }catch(IOException ex){
            if(wanted&&epoch==generation){
                error="Cannot listen on port "+LanSoundProtocol.PORT+": "+ex.getMessage();
                Log.e(TAG,error);
            }
        }finally{
            if(epoch==generation){
                ready=false;
                wanted=false;
                listener=null;
                fireChanged();
            }
        }
    }

    private static String localIPv4(){
        String fallback="";
        try{
            Enumeration<NetworkInterface> list=NetworkInterface.getNetworkInterfaces();
            while(list!=null&&list.hasMoreElements()){
                NetworkInterface network=list.nextElement();
                if(!network.isUp()||network.isLoopback())continue;
                Enumeration<InetAddress> ips=network.getInetAddresses();
                while(ips.hasMoreElements()){
                    InetAddress ip=ips.nextElement();
                    if(!(ip instanceof Inet4Address) ||
                       !AddressValidator.isPrivateLanIpv4(ip.getHostAddress()))continue;
                    if(network.getName().startsWith("wlan")||network.getName().startsWith("eth"))
                        return ip.getHostAddress();
                    if(fallback.isEmpty())fallback=ip.getHostAddress();
                }
            }
        }catch(Exception ex){Log.w(TAG,"Unable to determine LAN address",ex);}
        return fallback;
    }

    private static void closeQuietly(java.io.Closeable object){
        if(object!=null)try{object.close();}catch(IOException ignored){}
    }

    private static String line(BufferedInputStream in) throws IOException {
        ByteArrayOutputStream b=new ByteArrayOutputStream();
        for(;;){
            int c=in.read();
            if(c<0)throw new IOException("Unexpected end of HTTP headers");
            if(c=='\n')break;
            if(c=='\r')continue;
            if(b.size()>=4096)throw new IOException("HTTP header line too large");
            if(c==0)throw new IOException("Invalid HTTP header");
            b.write(c);
        }
        return b.toString("US-ASCII");
    }

    private static byte[] readExactly(InputStream in,int n) throws IOException {
        byte[] result=new byte[n];
        int offset=0;
        while(offset<n){
            int got=in.read(result,offset,n-offset);
            if(got<0)throw new IOException("Incomplete request body");
            if(got==0)continue;
            offset+=got;
        }
        return result;
    }

    private static byte[] utf8(String value){
        return value.getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] jsonError(String error){
        return utf8("{\"error\":"+JSONObject.quote(error)+"}");
    }

    private static String description(int code){
        switch(code){
            case 200:return "OK";
            case 201:return "Created";
            case 400:return "Bad Request";
            case 401:return "Unauthorized";
            case 403:return "Forbidden";
            case 404:return "Not Found";
            case 405:return "Method Not Allowed";
            case 409:return "Conflict";
            case 411:return "Length Required";
            case 413:return "Content Too Large";
            case 415:return "Unsupported Media Type";
            case 500:return "Internal Server Error";
            default:return "Error";
        }
    }

    private static void send(OutputStream raw,int status,String mime,byte[] body) throws IOException {
        BufferedOutputStream out=new BufferedOutputStream(raw);
        String headers="HTTP/1.1 "+status+" "+description(status)+"\r\n"
            +"Content-Type: "+mime+"\r\n"
            +"Content-Length: "+body.length+"\r\n"
            +"Cache-Control: no-store\r\n"
            +"X-Content-Type-Options: nosniff\r\n"
            +"X-Frame-Options: DENY\r\n"
            +"Referrer-Policy: no-referrer\r\n"
            +"Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; media-src blob:; base-uri 'none'; form-action 'self'\r\n"
            +"Connection: close\r\n\r\n";
        out.write(headers.getBytes(StandardCharsets.US_ASCII));
        out.write(body);
        out.flush();
    }

    private static void fail(OutputStream out,int status,String msg) throws IOException {
        send(out,status,"application/json; charset=utf-8",jsonError(msg));
    }

    private void respond(Socket client) throws IOException {
        BufferedInputStream in=new BufferedInputStream(client.getInputStream(),4096);
        OutputStream out=client.getOutputStream();
        String request=line(in);
        String[] first=request.split(" ");
        if(first.length!=3 || !first[2].startsWith("HTTP/1.")){
            fail(out,400,"Malformed HTTP request");return;
        }
        String method=first[0],path=first[1];
        if(!(method.equals("GET")||method.equals("POST")||method.equals("PUT")) ||
           !path.startsWith("/")||path.contains("?")||path.length()>100){
            fail(out,400,"Unsupported HTTP request");return;
        }
        Map<String,String> headers=new HashMap<>();
        for(int count=0;count<40;count++){
            String header=line(in);
            if(header.isEmpty())break;
            int colon=header.indexOf(':');
            if(colon<1){fail(out,400,"Invalid header");return;}
            String key=header.substring(0,colon).trim().toLowerCase(Locale.US);
            String value=header.substring(colon+1).trim();
            if(value.length()>2048||headers.containsKey(key)){
                fail(out,400,"Invalid header value");return;
            }
            headers.put(key,value);
            if(count==39){fail(out,400,"Too many request headers");return;}
        }
        if(!LanSoundProtocol.validHost(headers.get("host"))){
            fail(out,403,"Invalid LAN host");return;
        }
        if(headers.containsKey("transfer-encoding")){
            fail(out,400,"Chunked uploads not supported");return;
        }
        String origin=headers.get("origin");
        if(origin!=null&&!origin.equals("http://"+headers.get("host"))){
            fail(out,403,"Cross-origin requests are not allowed");return;
        }
        if(method.equals("GET")&&path.equals("/")){
            try(InputStream asset=context.getAssets().open("sound_manager.html")){
                ByteArrayOutputStream buffer=new ByteArrayOutputStream(12000);
                byte[] part=new byte[2048];int n;
                while((n=asset.read(part))!=-1)buffer.write(part,0,n);
                send(out,200,"text/html; charset=utf-8",buffer.toByteArray());
            }
            return;
        }
        if(!LanSoundProtocol.pinMatches(headers.get("x-glowdeck-pin"),code)||!ready||!wanted){
            fail(out,401,"Enter the eight-digit code shown on your Fire TV");return;
        }
        if(method.equals("GET")&&path.equals("/api/status")){
            try {
                send(out,200,"application/json; charset=utf-8",
                    utf8(statusJSON().toString()));
            }catch(Exception ex){fail(out,500,"Could not read sound settings");}
            return;
        }
        if(GameDayService.isRunning()||GameDayService.isStopping()||FollowService.isBusy()||FollowService.hasRecovery(context)){
            fail(out,409,"STOP BACKGROUND MONITOR before changing or previewing sounds");return;
        }
        if(method.equals("PUT")&&path.startsWith("/api/upload/")){
            String team=path.substring("/api/upload/".length());
            if(!LanSoundProtocol.validTeam(team)){
                fail(out,404,"Unsupported NHL team");return;
            }
            String mime=headers.get("content-type");
            if(mime==null || !(mime.startsWith("audio/wav") ||
                               mime.startsWith("application/octet-stream"))){
                fail(out,415,"Upload a canonical PCM WAV file");return;
            }
            int length=LanSoundProtocol.contentLength(headers.get("content-length"),
                LanSoundProtocol.MAX_UPLOAD);
            if(length<44){fail(out,length<0?413:400,"WAV must be 44–320044 bytes");return;}
            if(audio.isPlaying()){fail(out,409,"Stop the current audio preview first");return;}
            upload(in,out,team,length);
            return;
        }
        if(!method.equals("POST")){
            fail(out,404,"Unknown sound manager action");return;
        }
        int length=LanSoundProtocol.contentLength(headers.get("content-length"),MAX_JSON);
        if(length<0){fail(out,400,"Missing or oversized JSON request");return;}
        if(!"application/json".equals(headers.get("content-type"))){
            fail(out,415,"JSON required");return;
        }
        JSONObject data;
        try{data=new JSONObject(new String(readExactly(in,length),StandardCharsets.UTF_8));}
        catch(Exception ex){fail(out,400,"Invalid JSON");return;}
        try{
            switch(path){
                case "/api/settings":
                    settings(out,data);return;
                case "/api/team":
                    team(out,data);return;
                case "/api/preview":
                    preview(out,data);return;
                case "/api/stop":
                    audio.stop();
                    send(out,200,"application/json",utf8("{\"ok\":true}"));return;
                default:fail(out,404,"Unknown API method");
            }
        }catch(Exception ex){
            Log.e(TAG,"Sound API error",ex);
            fail(out,500,"Unable to update sound settings");
        }
    }

    private JSONObject statusJSON() throws Exception {
        JSONObject result=new JSONObject();
        result.put("enabled",prefs.getBoolean("enabled",false));
        result.put("volume",AudioProfile.volume(prefs.getInt("volume",50)));
        result.put("armed",GameDayService.isRunning()||FollowService.isBusy());
        JSONArray teams=new JSONArray();
        File folder=GameAudio.audioDirectory(context);
        for(String team:LanSoundProtocol.TEAMS){
            JSONObject one=new JSONObject();
            one.put("team",team);
            one.put("kind",AudioProfile.kind(prefs.getInt("kind."+team,AudioProfile.HORN)));
            String clip=prefs.getString("clip."+team,team);
            one.put("clip",AudioProfile.validName(clip)?clip:team);
            one.put("hasFile",new File(folder,team+".wav").isFile());
            one.put("color",String.format(Locale.US,"#%06X",NhlApi.teamColor(team)&0xFFFFFF));
            teams.put(one);
        }
        result.put("teams",teams);
        return result;
    }

    private void settings(OutputStream out,JSONObject value) throws Exception {
        SharedPreferences.Editor edit=prefs.edit();
        if(value.has("enabled")){
            if(!(value.get("enabled") instanceof Boolean)){
                fail(out,400,"enabled must be true or false");return;
            }
            boolean enabled=value.getBoolean("enabled");
            edit.putBoolean("enabled",enabled);
            if(!enabled)audio.stop();
        }
        if(value.has("volume")){
            if(!(value.get("volume") instanceof Number)){
                fail(out,400,"volume must be a number");return;
            }
            int volume=value.getInt("volume");
            if(volume<0||volume>100){fail(out,400,"Volume must be 0–100");return;}
            edit.putInt("volume",volume);
        }
        if(!edit.commit()){fail(out,500,"Could not persist settings");return;}
        send(out,200,"application/json",utf8("{\"ok\":true}"));
        fireChanged();
    }

    private void team(OutputStream out,JSONObject value) throws Exception {
        String team=value.optString("team","");
        int kind=value.optInt("kind",-1);
        if(!LanSoundProtocol.validTeam(team)||kind<0||kind>4){
            fail(out,400,"Select a valid NHL team and sound");return;
        }
        if(kind==AudioProfile.CUSTOM){
            File file=new File(GameAudio.audioDirectory(context),team+".wav");
            if(!file.isFile()){fail(out,409,"Upload a WAV for this team first");return;}
            byte[] head=new byte[44];
            try(FileInputStream in=new FileInputStream(file)){
                if(in.read(head)!=44 || !LanSoundProtocol.canonicalUpload(head,(int)file.length())){
                    fail(out,409,"Existing WAV is invalid; upload again");return;
                }
            }
        }
        if(!prefs.edit().putInt("kind."+team,kind).putString("clip."+team,team).commit()){
            fail(out,500,"Could not save team selection");return;
        }
        send(out,200,"application/json",utf8("{\"ok\":true}"));
        fireChanged();
    }

    private void preview(OutputStream out,JSONObject data) throws IOException {
        String team=data.optString("team","");
        if(!LanSoundProtocol.validTeam(team)){
            fail(out,400,"Select an NHL team");return;
        }
        if(!audio.play(team,true)){
            fail(out,409,"Audio is busy, muted, or unavailable");return;
        }
        send(out,200,"application/json",utf8("{\"ok\":true}"));
    }

    private void upload(InputStream in,OutputStream out,String team,int length) throws IOException {
        File folder=GameAudio.audioDirectory(context);
        if(!folder.isDirectory()&&!folder.mkdirs()){
            fail(out,500,"Cannot create audio folder");return;
        }
        File temp=new File(folder,team+".upload");
        File target=new File(folder,team+".wav");
        if(temp.exists()&&!temp.delete()){
            fail(out,500,"Cannot clear previous temp upload");return;
        }
        byte[] head=new byte[44];
        try(FileOutputStream file=new FileOutputStream(temp)){
            byte[] buffer=new byte[2048];
            int left=length,headIndex=0;
            while(left>0){
                int count=in.read(buffer,0,Math.min(buffer.length,left));
                if(count<0)throw new IOException("Truncated upload");
                if(count==0)continue;
                for(int i=0;i<count && headIndex<44;i++)head[headIndex++]=buffer[i];
                file.write(buffer,0,count);
                left-=count;
            }
            file.flush();
            file.getFD().sync();
        }catch(IOException ex){
            temp.delete();
            fail(out,400,"Upload interrupted or invalid");
            return;
        }
        if(!LanSoundProtocol.canonicalUpload(head,length)){
            temp.delete();
            fail(out,400,"WAV must be PCM mono 16000Hz 16-bit, up to 10 seconds");
            return;
        }
        if(!temp.renameTo(target)){
            temp.delete();
            fail(out,500,"Could not install new team WAV");return;
        }
        if(!prefs.edit().putString("clip."+team,team)
                .putInt("kind."+team,AudioProfile.CUSTOM).commit()){
            fail(out,500,"Audio file saved but setting could not be updated");return;
        }
        Log.i(TAG,"Imported WAV for "+team+" ("+length+" bytes)");
        send(out,201,"application/json",utf8("{\"ok\":true,\"team\":\""+team+"\"}"));
        fireChanged();
    }
}
