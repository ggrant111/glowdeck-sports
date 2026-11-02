package com.gkowdeck.gameday.firetv;
import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.InetAddress;
/** Dedicated bounded sender: NHL HTTP polling must never stall animation frames. */
final class DdpStream implements AutoCloseable {
    private volatile boolean stop;
    private Thread thread;
    private DatagramSocket socket;
    volatile String error="";
    synchronized void start(String ip,int count,int event,int color)throws Exception{
        close();
        if(!AddressValidator.isPrivateLanIpv4(ip)||count<1||count>EventEffects.MAX_PIXELS)throw new IllegalArgumentException("DDP requires 1–600 LEDs on a private LAN");
        InetAddress address=InetAddress.getByName(ip);socket=new DatagramSocket();stop=false;error="";
        final DatagramSocket sender=socket;
        thread=new Thread(()->{
            long start=System.nanoTime();int seq=1;
            try{
                while(!stop){
                    long ms=(System.nanoTime()-start)/1_000_000;if(ms>=8000)break;
                    for(int first=0;first<count&&!stop;first+=EventEffects.CHUNK){
                        byte[] b=EventEffects.packet(event,color,count,first,ms,seq);
                        sender.send(new DatagramPacket(b,b.length,address,4048));
                    }
                    seq=seq%15+1;
                    Thread.sleep(50); // cap at 20fps; no catch-up bursts after scheduling delays
                }
            }catch(InterruptedException ignored){Thread.currentThread().interrupt();}
             catch(Exception ex){if(!stop)error=ex.toString();}
            finally{sender.close();}
        },"GameDay-DDP");thread.start();
    }
    public synchronized void close(){
        stop=true;if(socket!=null)socket.close();
        if(thread!=null){thread.interrupt();try{thread.join(1000);}catch(InterruptedException ex){Thread.currentThread().interrupt();}}
        thread=null;socket=null;
    }
}
