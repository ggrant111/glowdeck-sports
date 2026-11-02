package com.gkowdeck.gameday.firetv;
import org.json.JSONObject;
/** Snapshot is persisted by caller before start(). Whole-device realtime mode. */
final class DdpScene {
    static int count(String ip)throws Exception{
        JSONObject info=WledApi.info(ip);
        if(info.optBoolean("live",false))throw new Exception("WLED already has a realtime sender");
        int n=info.getJSONObject("leds").optInt("count",0);
        if(n<1||n>EventEffects.MAX_PIXELS)throw new Exception("DDP supports 1–600 LEDs per device");
        return n;
    }
    static void start(DdpStream stream,String ip,int n,int kind,int color)throws Exception{
        WledApi.send(ip,new JSONObject().put("on",true).put("bri",160).put("tt",0).put("udpn",new JSONObject().put("nn",true)));
        stream.start(ip,n,kind,color);
    }
}
