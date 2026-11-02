package com.gkowdeck.gameday.firetv;

/** Restrict WLED test traffic to explicitly selected RFC1918 IPv4 hosts on port 80. */
public final class AddressValidator {
    private AddressValidator() {}
    public static boolean isPrivateLanIpv4(String value) {
        if(value==null || value.length()>15 || value.length()<7 || !value.equals(value.trim()))return false;
        String[] items=value.split("\\.",-1);
        if(items.length!=4)return false;
        int[] n=new int[4];
        for(int i=0;i<4;i++) {
            if(items[i].isEmpty()||items[i].length()>3)return false;
            for(int j=0;j<items[i].length();j++)if(items[i].charAt(j)<'0'||items[i].charAt(j)>'9')return false;
            try {n[i]=Integer.parseInt(items[i]);} catch(NumberFormatException e){return false;}
            if(n[i]<0||n[i]>255)return false;
        }
        // Exclude gateway/network and directed broadcast addresses from the simple LAN test mode.
        if(n[3]==0 || n[3]==255)return false;
        return n[0]==10 || (n[0]==172 && n[1]>=16 && n[1]<=31) || (n[0]==192 && n[1]==168);
    }
}
