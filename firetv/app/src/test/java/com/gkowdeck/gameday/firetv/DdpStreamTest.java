package com.gkowdeck.gameday.firetv;
import java.net.*;
import java.util.*;
import org.junit.Test;
import org.junit.Assume;
import static org.junit.Assert.*;
public class DdpStreamTest {
 @Test public void sendsChunkedFramesAndStops()throws Exception{
  String local=null;
  for(NetworkInterface n:Collections.list(NetworkInterface.getNetworkInterfaces()))
   for(InetAddress a:Collections.list(n.getInetAddresses()))
    if(AddressValidator.isPrivateLanIpv4(a.getHostAddress()))local=a.getHostAddress();
  Assume.assumeNotNull(local);
  try(DatagramSocket receiver=new DatagramSocket(4048,InetAddress.getByName(local));DdpStream sender=new DdpStream()){
   receiver.setSoTimeout(1500);sender.start(local,600,0,0xffffff);
   byte[] one=new byte[1500],two=new byte[1500];DatagramPacket a=new DatagramPacket(one,one.length),b=new DatagramPacket(two,two.length);
   receiver.receive(a);receiver.receive(b);
   assertEquals(1450,a.getLength());assertEquals(370,b.getLength());assertEquals(0x40,one[0]);assertEquals(0x41,two[0]);assertEquals(one[1],two[1]);assertEquals(160,one[10]&255);
   sender.close();receiver.setSoTimeout(150);
   // Drain at most a few already-queued datagrams. No producer remains after close.
   int drained=0;try{while(drained<10){receiver.receive(a);drained++;}}catch(SocketTimeoutException expected){}
   assertTrue("sender continued after close",drained<10);
  }
 }
}
