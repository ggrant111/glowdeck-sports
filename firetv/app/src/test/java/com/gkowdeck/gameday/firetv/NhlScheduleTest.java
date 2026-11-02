package com.gkowdeck.gameday.firetv;
import org.junit.Test;
import org.json.JSONObject;
import static org.junit.Assert.*;
public class NhlScheduleTest {
 private JSONObject game(String state)throws Exception{return new JSONObject("{\"id\":2026020001,\"gameState\":\""+state+"\",\"awayTeam\":{\"id\":1,\"abbrev\":\"UTA\"},\"homeTeam\":{\"id\":2,\"abbrev\":\"BOS\"}}");}
 @Test public void pregameWithoutPlaysIsValid()throws Exception{NhlApi.Pbp p=NhlApi.parsePlays(2026020001L,game("FUT"));assertEquals("FUT",p.state);assertTrue(p.plays.isEmpty());}
 @Test(expected=Exception.class) public void liveWithoutPlaysIsRejected()throws Exception{NhlApi.parsePlays(2026020001L,game("LIVE"));}
 @Test(expected=Exception.class) public void wrongGameRejected()throws Exception{NhlApi.parsePlays(2026020002L,game("FUT"));}
 @Test public void cancellationOverridesGameState()throws Exception{JSONObject g=game("FUT");g.put("gameScheduleState","CNCL");assertEquals("CNCL",NhlApi.parsePlays(2026020001L,g).state);}
 @Test public void emptyScheduleIsValid()throws Exception{assertTrue(NhlApi.parseScores(new JSONObject("{\"currentDate\":\"2026-11-03\",\"games\":[]}")).games.isEmpty());}
 @Test(expected=Exception.class) public void missingScheduleRejected()throws Exception{NhlApi.parseScores(new JSONObject("{}"));}
 @Test public void timeFormatChoice(){
  boolean old=NhlApi.hour24;java.util.TimeZone zone=java.util.TimeZone.getDefault();java.util.Locale locale=java.util.Locale.getDefault();
  try{java.util.TimeZone.setDefault(java.util.TimeZone.getTimeZone("UTC"));java.util.Locale.setDefault(java.util.Locale.US);
   NhlApi.Game g=new NhlApi.Game();g.startTime="2026-11-03T00:05:00Z";
   NhlApi.hour24=false;assertTrue(g.localStart().endsWith("12:05 AM"));
   NhlApi.hour24=true;assertTrue(g.localStart().endsWith("00:05"));
   g.startTime="2026-11-03T12:00:00Z";NhlApi.hour24=false;assertTrue(g.localStart().endsWith("12:00 PM"));
  }finally{NhlApi.hour24=old;java.util.TimeZone.setDefault(zone);java.util.Locale.setDefault(locale);}
 }

}
