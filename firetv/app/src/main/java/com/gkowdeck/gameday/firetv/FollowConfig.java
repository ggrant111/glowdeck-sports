package com.gkowdeck.gameday.firetv;
import org.json.*;
import java.util.*;
/** Persisted preferences only: never persists an armed state. */
final class FollowConfig {
    static final int LIMIT=4;
    final List<Game> games=new ArrayList<>();
    final String[] targets={"",""};
    static final class Game {
        long id; String away="?",home="?";
        boolean display=true,sound=false,ddp=false;
        int outputs=0,mode=0,delay=0,mask=127;
    }
    static FollowConfig parse(String raw)throws JSONException {
        FollowConfig c=new FollowConfig();JSONObject d=new JSONObject(raw);
        JSONArray t=d.optJSONArray("targets"),a=d.optJSONArray("games");
        if(t!=null)for(int i=0;i<2;i++)c.targets[i]=t.optString(i,"");
        if(a!=null)for(int i=0;i<a.length()&&c.games.size()<LIMIT;i++){
            JSONObject o=a.getJSONObject(i);Game g=new Game();g.id=o.optLong("id");
            if(g.id<2000000000L||g.id>2999999999L)continue;
            boolean duplicate=false;for(Game prior:c.games)if(prior.id==g.id)duplicate=true;if(duplicate)continue;
            g.away=o.optString("away","?");g.home=o.optString("home","?");g.display=o.optBoolean("display",true);g.sound=o.optBoolean("sound",false);
            g.ddp=o.optBoolean("ddp",false);g.outputs=o.optInt("targets",0)&3;g.mode=CelebrationSelection.normalize(o.optInt("mode",0));
            g.delay=MonitorRules.delaySeconds(o.optInt("delay",0));g.mask=o.optInt("mask",127)&127;c.games.add(g);
        }return c;
    }
    String json()throws JSONException {
        JSONObject d=new JSONObject();JSONArray a=new JSONArray(),t=new JSONArray();for(String ip:targets)t.put(ip);d.put("targets",t);
        for(Game g:games)a.put(new JSONObject().put("id",g.id).put("away",g.away).put("home",g.home).put("display",g.display).put("sound",g.sound).put("ddp",g.ddp).put("targets",g.outputs).put("mode",g.mode).put("delay",g.delay).put("mask",g.mask));
        return d.put("games",a).toString();
    }
    void validate()throws Exception {
        if(games.isEmpty())throw new Exception("Follow at least one game");int used=0;for(Game g:games)used|=g.outputs;
        if(used==3&&targets[0].equals(targets[1]))throw new Exception("Targets A and B must be different devices");
        for(int i=0;i<2;i++)if((used&(1<<i))!=0&&!AddressValidator.isPrivateLanIpv4(targets[i]))throw new Exception("Choose valid WLED IPv4 for target "+(i==0?"A":"B"));
    }
}
