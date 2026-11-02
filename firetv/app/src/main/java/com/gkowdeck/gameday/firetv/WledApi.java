package com.gkowdeck.gameday.firetv;

import org.json.JSONArray;
import org.json.JSONObject;

final class WledApi {
    static String url(String ip,String path)throws Exception {
        if(!AddressValidator.isPrivateLanIpv4(ip))throw new Exception("Use an RFC1918 private WLED IPv4 address");
        return "http://"+ip+path;
    }
    static JSONObject info(String ip)throws Exception {
        JSONObject o=Net.get(url(ip,"/json/info"));
        if(!o.has("ver")||!o.has("leds"))throw new Exception("Response was not recognized as WLED");
        return o;
    }
    static JSONObject state(String ip)throws Exception {return Net.get(url(ip,"/json/state"));}
    static void send(String ip,JSONObject cmd)throws Exception {Net.post(url(ip,"/json/state"),cmd);}
    static JSONObject previewColor(int color,int brightness)throws Exception {
        JSONObject obj=new JSONObject();obj.put("on",true);obj.put("bri",Math.max(1,Math.min(160,brightness)));
        obj.put("tt",0);obj.put("udpn",new JSONObject().put("nn",true));
        JSONObject segment=new JSONObject();segment.put("id",0);segment.put("on",true);segment.put("frz",false);
        segment.put("fx",0);segment.put("pal",0);
        segment.put("col",new JSONArray().put(new JSONArray().put((color>>16)&255).put((color>>8)&255).put(color&255)));
        obj.put("seg",new JSONArray().put(segment));return obj;
    }
    static JSONObject snapshotRestore(JSONObject original)throws Exception {
        if(original.optInt("lor",0)!=0)throw new Exception("Disable WLED realtime override first");
        if(!original.has("on")||!original.has("seg"))throw new Exception("Incomplete WLED state; preview not allowed");
        JSONObject restore=new JSONObject();restore.put("on",original.optBoolean("on",false));
        restore.put("bri",Math.max(1,Math.min(255,original.optInt("bri",128))));
        restore.put("live",false);restore.put("tt",0);restore.put("udpn",new JSONObject().put("nn",true));
        JSONArray source=original.getJSONArray("seg");JSONObject seg0=null;
        for(int i=0;i<source.length();i++){
            JSONObject part=source.optJSONObject(i);
            if(part!=null&&part.optInt("id",i)==0){seg0=part;break;}
        }
        if(seg0==null)throw new Exception("WLED segment 0 missing");
        JSONObject copy=new JSONObject();copy.put("id",0);
        String[] keep={"on","bri","fx","sx","ix","pal","col","frz","sel","cct","rev","mi"};
        for(String key:keep)if(seg0.has(key)&&!seg0.isNull(key))copy.put(key,seg0.get(key));
        restore.put("seg",new JSONArray().put(copy));return restore;
    }
}
