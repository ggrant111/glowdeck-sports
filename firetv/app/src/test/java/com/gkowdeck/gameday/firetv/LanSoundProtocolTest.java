package com.gkowdeck.gameday.firetv;

import org.junit.Test;
import static org.junit.Assert.*;

public class LanSoundProtocolTest {
    @Test public void onlyCurrentNhlTeamsCanBeEdited(){
        assertEquals(32,LanSoundProtocol.TEAMS.length);
        assertTrue(LanSoundProtocol.validTeam("ANA"));
        assertTrue(LanSoundProtocol.validTeam("UTA"));
        assertTrue(LanSoundProtocol.validTeam("WSH"));
        assertFalse(LanSoundProtocol.validTeam("../ANA"));
        assertFalse(LanSoundProtocol.validTeam("ARI"));
        assertFalse(LanSoundProtocol.validTeam("ana"));
        assertFalse(LanSoundProtocol.validTeam(""));
    }
    @Test public void pairingCodeIsRequiredAndComparedExactly(){
        assertTrue(LanSoundProtocol.pinMatches("12345678","12345678"));
        assertFalse(LanSoundProtocol.pinMatches("12345679","12345678"));
        assertFalse(LanSoundProtocol.pinMatches("1234567","12345678"));
        assertFalse(LanSoundProtocol.pinMatches(null,"12345678"));
        assertFalse(LanSoundProtocol.pinMatches("x2345678","12345678"));
    }
    @Test public void rejectInvalidOrOversizedBodies(){
        assertEquals(44,LanSoundProtocol.contentLength("44",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(320044,LanSoundProtocol.contentLength("320044",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(-1,LanSoundProtocol.contentLength("320045",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(-1,LanSoundProtocol.contentLength("-1",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(-1,LanSoundProtocol.contentLength("0",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(-1,LanSoundProtocol.contentLength("9999999999999",LanSoundProtocol.MAX_UPLOAD));
        assertEquals(-1,LanSoundProtocol.contentLength("44x",LanSoundProtocol.MAX_UPLOAD));
    }
    @Test public void onlyPrivateLanHostIsAccepted(){
        assertTrue(LanSoundProtocol.validHost("192.168.1.24:8788"));
        assertTrue(LanSoundProtocol.validHost("10.0.0.12:8788"));
        assertTrue(LanSoundProtocol.validHost("localhost:8788"));
        assertFalse(LanSoundProtocol.validHost("8.8.8.8:8788"));
        assertFalse(LanSoundProtocol.validHost("evil.example:8788"));
        assertFalse(LanSoundProtocol.validHost("192.168.1.24:8080"));
        assertFalse(LanSoundProtocol.validHost("192.168.1.24:8788.evil.example"));
    }
    private static void u16(byte[] a,int i,int v){a[i]=(byte)v;a[i+1]=(byte)(v>>8);}
    private static void u32(byte[] a,int i,int v){
        for(int j=0;j<4;j++)a[i+j]=(byte)(v>>(8*j));
    }
    @Test public void wavImportIsStrictlyCanonical(){
        byte[] h=new byte[44];
        System.arraycopy("RIFF".getBytes(),0,h,0,4);
        u32(h,4,36+32000);
        System.arraycopy("WAVEfmt ".getBytes(),0,h,8,8);
        u32(h,16,16);u16(h,20,1);u16(h,22,1);
        u32(h,24,16000);u32(h,28,32000);u16(h,32,2);u16(h,34,16);
        System.arraycopy("data".getBytes(),0,h,36,4);u32(h,40,32000);
        assertTrue(LanSoundProtocol.canonicalUpload(h,32044));
        assertFalse(LanSoundProtocol.canonicalUpload(h,32046));
        assertFalse(LanSoundProtocol.canonicalUpload(h,100));
        u16(h,22,2);
        assertFalse(LanSoundProtocol.canonicalUpload(h,32044));
    }
}
