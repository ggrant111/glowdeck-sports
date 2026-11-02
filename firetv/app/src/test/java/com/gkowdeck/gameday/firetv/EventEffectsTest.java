package com.gkowdeck.gameday.firetv;
import org.junit.Test;
import static org.junit.Assert.*;
public class EventEffectsTest {
 @Test public void packetOffsetsAndPush(){
 byte[] b=EventEffects.packet(0,0x6cace4,600,0,0,15);assertEquals(1450,b.length);assertEquals(0x40,b[0]);assertEquals(15,b[1]);assertEquals(11,b[2]);assertEquals(1,b[3]);assertEquals(5,b[8]);assertEquals(160,b[9]&255);assertEquals(67,b[10]);
 b=EventEffects.packet(0,0x6cace4,600,480,0,1);assertEquals(370,b.length);assertEquals(0x41,b[0]);assertEquals(5,b[6]);assertEquals(160,b[7]&255);
 }
 @Test(expected=IllegalArgumentException.class) public void rejectOversize(){EventEffects.packet(0,0,601,0,0,0);}
 @Test public void manpower(){assertEquals(0,EventEffects.advantage("1551"));assertEquals(1,EventEffects.advantage("1541"));assertEquals(2,EventEffects.advantage("1451"));assertEquals(-1,EventEffects.advantage("0651"));assertEquals(-1,EventEffects.advantage("1550"));assertEquals(-1,EventEffects.advantage(""));}
 @Test public void renderAllPatterns(){for(int e=0;e<7;e++)for(int i=0;i<600;i++)assertEquals(0,EventEffects.pixel(e,0xabcdef,i,600,1975)&0xff000000);assertEquals(0,EventEffects.pixel(3,0xffffff,0,10,8000));}
}
