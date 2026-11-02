package com.gkowdeck.gameday.firetv;

import org.json.JSONArray;
import org.json.JSONObject;
import java.util.ArrayList;
import java.util.List;

final class NhlApi {
    static final String ROOT="https://api-web.nhle.com/v1";
    static final class Game {
        long id;String away,home,state,clock,startTime="";int awayId,homeId,awayScore,homeScore,period;
        boolean live(){return "LIVE".equals(state)||"CRIT".equals(state);}
        boolean upcoming(){return "FUT".equals(state)||"PRE".equals(state)||"PPD".equals(state);}
        String localStart(){
            try {java.text.SimpleDateFormat utc=new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'",java.util.Locale.US);
                utc.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
                return java.text.DateFormat.getDateTimeInstance(java.text.DateFormat.SHORT,java.text.DateFormat.SHORT).format(utc.parse(startTime));
            }catch(Exception e){return "Start time TBD";}
        }
        String status(){
            if(live())return "LIVE P"+period+(clock.isEmpty()?"":"  "+clock);
            if("OFF".equals(state)||"FINAL".equals(state))return "FINAL";
            if("FUT".equals(state)||"PRE".equals(state))return "UPCOMING • "+localStart();
            return state;
        }
        String score(){return ("FUT".equals(state)||"PRE".equals(state)) ? "vs" : awayScore+" – "+homeScore;}
        String matchup(){return away+" @ "+home;}
    }
    static class Board {String date; List<Game> games=new ArrayList<>();}
    static Board scores()throws Exception {return scores(0);}
    static Board scores(int day)throws Exception {
        java.util.Calendar c=java.util.Calendar.getInstance();c.add(java.util.Calendar.DAY_OF_YEAR,Math.max(0,Math.min(day,6)));
        String date=new java.text.SimpleDateFormat("yyyy-MM-dd",java.util.Locale.US).format(c.getTime());
        return parseScores(Net.get(ROOT+"/score/"+date));
    }
    static Board parseScores(JSONObject root)throws Exception {
        Board board=new Board();board.date=root.optString("currentDate","Today");
        JSONArray games=root.optJSONArray("games");if(games==null)throw new Exception("NHL scoreboard missing games array");
        for(int i=0;i<Math.min(games.length(),22);i++) {
            JSONObject o=games.optJSONObject(i);if(o==null)continue;
            Game g=new Game();g.id=o.optLong("id"); if(g.id==0)continue;
            JSONObject a=o.optJSONObject("awayTeam"),h=o.optJSONObject("homeTeam");
            if(a==null||h==null)continue;
            g.away=a.optString("abbrev","AWY");g.home=h.optString("abbrev","HME");
            g.awayId=a.optInt("id",0);g.homeId=h.optInt("id",0);
            g.awayScore=a.optInt("score",0);g.homeScore=h.optInt("score",0);
            g.startTime=o.optString("startTimeUTC","");
            g.state=o.optString("gameScheduleState","");if(!"PPD".equals(g.state)&&!"CNCL".equals(g.state))g.state=o.optString("gameState","UNKNOWN");g.period=o.optInt("period",0);
            JSONObject clock=o.optJSONObject("clock");g.clock=clock==null?"":clock.optString("timeRemaining","");
            board.games.add(g);
        }
        return board;
    }
    static class Play {
        int eventId,ownerId,period;String type,time,description,periodType,situation;
        boolean goal(){return "goal".equalsIgnoreCase(type)&&!"SO".equals(periodType);}
    }
    static class Pbp {String state; boolean finished(){return "OFF".equals(state)||"FINAL".equals(state);} long gameId;int homeId,awayId; List<Play> plays=new ArrayList<>();}
    static Pbp plays(long id)throws Exception {
        if(id<2000000000L || id>2999999999L)throw new Exception("Select a valid NHL game first");
        return parsePlays(id,Net.get(ROOT+"/gamecenter/"+id+"/play-by-play"));
    }
    static Pbp parsePlays(long id,JSONObject o)throws Exception {
        Pbp pbp=new Pbp();pbp.gameId=id;pbp.state=o.optString("gameScheduleState","");if(!"PPD".equals(pbp.state)&&!"CNCL".equals(pbp.state))pbp.state=o.optString("gameState","");
        if(o.optLong("id",0)!=id)throw new Exception("NHL game ID mismatch");
        JSONObject h=o.optJSONObject("homeTeam"),a=o.optJSONObject("awayTeam");
        pbp.homeId=h==null?0:h.optInt("id",0);pbp.awayId=a==null?0:a.optInt("id",0);
        JSONArray list=o.optJSONArray("plays"); if(list==null){if("FUT".equals(pbp.state)||"PRE".equals(pbp.state)||"PPD".equals(pbp.state)||"CNCL".equals(pbp.state))list=new JSONArray();else throw new Exception("No NHL play-by-play array in response");}
        for(int i=0;i<list.length();i++){
            JSONObject p=list.optJSONObject(i);if(p==null)continue;
            Play x=new Play(); x.eventId=p.optInt("eventId",0);
            x.type=p.optString("typeDescKey","");
            x.period=p.optJSONObject("periodDescriptor")==null?0:p.optJSONObject("periodDescriptor").optInt("number",0);
            x.periodType=p.optJSONObject("periodDescriptor")==null?"":p.optJSONObject("periodDescriptor").optString("periodType","");
            x.situation=p.optString("situationCode","");
            x.time=p.optString("timeInPeriod","");
            JSONObject d=p.optJSONObject("details");x.ownerId=d==null?0:d.optInt("eventOwnerTeamId",0);
            x.description=x.type.toUpperCase()+" P"+x.period+" "+x.time;
            pbp.plays.add(x);
        }
        return pbp;
    }
    static int teamColor(String team) {
        // Static primary colors, test-purpose mapping. Team branding may evolve.
        switch(team) {
            case "UTA": return 0x6CACE4;case "BOS":return 0xFFB81C;case "COL":return 0x6F263D;
            case "VGK":return 0xB4975A;case "NYR":return 0x0038A8;case "TOR":return 0x00205B;
            case "EDM":return 0xFF4C00;case "DET":return 0xCE1126;case "MTL":return 0xAF1E2D;
            case "SEA":return 0x99D9D9;case "CHI":return 0xCF0A2C;case "DAL":return 0x00843D;
            case "TBL":return 0x002868;case "CAR":return 0xCC0000;case "PIT":return 0xFCB514;
            case "LAK":return 0xA2AAAD;case "PHI":return 0xF74902;case "STL":return 0x002F87;
            case "ANA":return 0xF47A38;case "BUF":return 0x003087;case "CGY":return 0xC8102E;
            case "CBJ":return 0x002654;case "FLA":return 0xC8102E;case "MIN":return 0x154734;
            case "NSH":return 0xFFB81C;case "NJD":return 0xCE1126;case "NYI":return 0x00539B;
            case "OTT":return 0xE31837;case "SJS":return 0x006D75;case "VAN":return 0x00205B;
            case "WSH":return 0xC8102E;case "WPG":return 0x041E42;
            default:return 0x31837A;
        }
    }
}
