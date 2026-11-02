package com.gkowdeck.gameday.firetv;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** Dependency-free input validation shared by LAN sound server and unit tests. */
final class LanSoundProtocol {
    static final int PORT=8788;
    static final int MAX_UPLOAD=320044; // 44-byte header + 10 seconds 16kHz mono 16-bit
    static final String[] TEAMS={
        "ANA","BOS","BUF","CAR","CBJ","CGY","CHI","COL",
        "DAL","DET","EDM","FLA","LAK","MIN","MTL","NJD",
        "NSH","NYI","NYR","OTT","PHI","PIT","SEA","SJS",
        "STL","TBL","TOR","UTA","VAN","VGK","WPG","WSH"
    };
    private static final Set<String> ALLOWED=Collections.unmodifiableSet(
        new HashSet<>(Arrays.asList(TEAMS)));

    static boolean validTeam(String team) {
        return team!=null && ALLOWED.contains(team);
    }

    static boolean pinMatches(String provided,String expected) {
        if(provided==null||expected==null||!provided.matches("[0-9]{8}"))return false;
        return MessageDigest.isEqual(provided.getBytes(StandardCharsets.US_ASCII),
            expected.getBytes(StandardCharsets.US_ASCII));
    }

    static int contentLength(String header,int max) {
        if(header==null||!header.matches("[0-9]{1,9}"))return -1;
        try {
            long n=Long.parseLong(header);
            return n>0&&n<=max?(int)n:-1;
        }catch(NumberFormatException ex){return -1;}
    }

    static boolean validHost(String host) {
        if(host==null)return false;
        String[] parts=host.split(":",-1);
        if(parts.length!=2||!parts[1].equals(String.valueOf(PORT)))return false;
        String ip=parts[0];
        return "localhost".equals(ip)||"127.0.0.1".equals(ip)
            ||AddressValidator.isPrivateLanIpv4(ip);
    }

    static boolean canonicalUpload(byte[] header,int fileBytes) {
        int data=AudioProfile.wavBytes(header,fileBytes);
        if(data<=0 || fileBytes!=44+data || header==null || header.length<44)return false;
        long riffSize=(header[4]&255L)|((header[5]&255L)<<8)
            |((header[6]&255L)<<16)|((header[7]&255L)<<24);
        return riffSize==(long)fileBytes-8;
    }
}
