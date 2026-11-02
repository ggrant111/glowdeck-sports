package com.gkowdeck.gameday.firetv;

import java.util.Locale;

/** Pure Java sound profile and strict canonical WAV validation; unit testable without Android. */
final class AudioProfile {
    static final int OFF=0, HORN=1, DOUBLE=2, SIREN=3, CUSTOM=4;
    static int kind(int raw){return raw>=0 && raw<=4?raw:OFF;}
    static int volume(int raw){return Math.max(0,Math.min(100,raw));}
    static boolean validName(String s){
        return s!=null && s.matches("[A-Z0-9_-]{1,24}");
    }
    static String name(int kind){
        switch(kind(kind)){case HORN:return "STADIUM HORN";case DOUBLE:return "DOUBLE HORN";
            case SIREN:return "SIREN";case CUSTOM:return "CUSTOM WAV";default:return "OFF";}
    }
    private static int u16(byte[] b,int i){return (b[i]&255)|((b[i+1]&255)<<8);}
    private static long u32(byte[] b,int i){
        return (b[i]&255L)|((b[i+1]&255L)<<8)|((b[i+2]&255L)<<16)|((b[i+3]&255L)<<24);
    }
    static int wavBytes(byte[] b,long fileLength) {
        if(b==null||b.length<44||fileLength<44)return -1;
        if(b[0]!='R'||b[1]!='I'||b[2]!='F'||b[3]!='F'||b[8]!='W'||b[9]!='A'||
            b[10]!='V'||b[11]!='E'||b[12]!='f'||b[13]!='m'||b[14]!='t'||b[15]!=' '||
            b[36]!='d'||b[37]!='a'||b[38]!='t'||b[39]!='a')return -1;
        if(u32(b,16)!=16 || u16(b,20)!=1 || u16(b,22)!=1 || u32(b,24)!=16000 ||
            u32(b,28)!=32000 || u16(b,32)!=2 || u16(b,34)!=16)return -1;
        long size=u32(b,40);
        if(size<=0||size>320000||size%2!=0||size>fileLength-44)return -1;
        return (int)size;
    }
}
