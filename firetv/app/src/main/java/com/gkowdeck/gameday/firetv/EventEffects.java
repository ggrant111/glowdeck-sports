package com.gkowdeck.gameday.firetv;
/** Portable DDP/effect contract. No Android classes; unit testable. */
final class EventEffects {
    static final int GOAL=0, POWER_PLAY=1, PERIOD_START=2, PERIOD_END=3, GAME_END=4, OVERTIME=5, SHOOTOUT=6;
    static final String[] NAMES={"GOAL","POWER PLAY","PERIOD START","PERIOD END","GAME END","OVERTIME","SHOOTOUT"};
    static final int MAX_PIXELS=600, CHUNK=480;
    static int priority(int e){return new int[]{100,30,40,20,90,60,70}[e];}
    static int pixel(int e,int color,int i,int count,long ms){
        int level=255,t=(int)(ms/50);
        switch(e){
        case GOAL:level=(i+t*2)%24<8?255:45;break;
        case POWER_PLAY:level=(i+t)%32<10?220:30;break;
        case PERIOD_START:level=i<=t*count/80?230:20;break;
        case PERIOD_END:level=255-(int)Math.min(255,ms*255/8000);break;
        case GAME_END:level=(i*13+t*3)%47<15?255:70;break;
        case OVERTIME:int phase=t%40;level=45+(phase<20?phase:40-phase)*10;break;
        case SHOOTOUT:level=(i/12+t/12)%2!=0?240:35;break;
        default:throw new IllegalArgumentException("Unknown effect");
        }
        return (((color>>16)&255)*level/255<<16)|(((color>>8)&255)*level/255<<8)|((color&255)*level/255);
    }
    static byte[] packet(int e,int color,int count,int first,long ms,int sequence){
        if(count<1||count>MAX_PIXELS||first<0||first>=count)throw new IllegalArgumentException("LED range");
        int n=Math.min(CHUNK,count-first),bytes=n*3,offset=first*3;
        byte[] out=new byte[10+bytes];out[0]=(byte)(0x40|(first+n==count?1:0));out[1]=(byte)(sequence&15);
        out[2]=0x0b;out[3]=1;out[4]=(byte)(offset>>>24);out[5]=(byte)(offset>>>16);out[6]=(byte)(offset>>>8);out[7]=(byte)offset;
        out[8]=(byte)(bytes>>>8);out[9]=(byte)bytes;
        for(int k=0;k<n;k++){int c=pixel(e,color,first+k,count,ms);out[10+k*3]=(byte)(((c>>16)&255)*160/255);out[11+k*3]=(byte)(((c>>8)&255)*160/255);out[12+k*3]=(byte)((c&255)*160/255);}
        return out;
    }
    static int advantage(String s){
        if(s==null||s.length()!=4||s.charAt(0)!='1'||s.charAt(3)!='1'||s.charAt(1)<'3'||s.charAt(1)>'5'||s.charAt(2)<'3'||s.charAt(2)>'5')return -1;
        return s.charAt(1)>s.charAt(2)?1:s.charAt(2)>s.charAt(1)?2:0;
    }
    static int periodStart(String type){return "OT".equals(type)?OVERTIME:"SO".equals(type)?SHOOTOUT:PERIOD_START;}
}
