package com.gkowdeck.gameday.firetv;

import org.junit.Test;
import static org.junit.Assert.*;

public class AudioProfileTest {
    @Test public void kindsAndVolumeAreSafe(){
        assertEquals(0,AudioProfile.kind(-1));assertEquals(0,AudioProfile.kind(99));
        assertEquals(4,AudioProfile.kind(4));
        assertEquals(0,AudioProfile.volume(-200));assertEquals(100,AudioProfile.volume(300));
    }
    @Test public void filenamesAreSafe(){
        assertTrue(AudioProfile.validName("ANA_GOAL"));
        assertTrue(AudioProfile.validName("HORN-3"));
        assertFalse(AudioProfile.validName("../foo"));
        assertFalse(AudioProfile.validName("goal.wav"));
        assertFalse(AudioProfile.validName("AAAAAAAAAAAAAAAAAAAAAAAAAA"));
    }
    private static void u16(byte[] a,int p,int n){a[p]=(byte)n;a[p+1]=(byte)(n>>8);}
    private static void u32(byte[] a,int p,int n){for(int i=0;i<4;i++)a[p+i]=(byte)(n>>(8*i));}
    static byte[] header(int frames){
        byte[] b=new byte[44];
        System.arraycopy("RIFF".getBytes(),0,b,0,4);u32(b,4,36+frames*2);
        System.arraycopy("WAVEfmt ".getBytes(),0,b,8,8);u32(b,16,16);
        u16(b,20,1);u16(b,22,1);u32(b,24,16000);
        u32(b,28,32000);u16(b,32,2);u16(b,34,16);
        System.arraycopy("data".getBytes(),0,b,36,4);u32(b,40,frames*2);
        return b;
    }
    @Test public void validWavHeader(){
        assertEquals(32000,AudioProfile.wavBytes(header(16000),32044));
    }
    @Test public void rejectStereoFloatAndTruncated(){
        byte[] b=header(16000);u16(b,22,2);assertEquals(-1,AudioProfile.wavBytes(b,32044));
        b=header(16000);u16(b,20,3);assertEquals(-1,AudioProfile.wavBytes(b,32044));
        assertEquals(-1,AudioProfile.wavBytes(header(16000),100));
        assertEquals(-1,AudioProfile.wavBytes(header(160001),320046));
    }
}
