package com.gkowdeck.gameday.firetv;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
public final class MonitorResumeReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context,Intent event) {
        if(!Intent.ACTION_BOOT_COMPLETED.equals(event.getAction()))return;
        SharedPreferences lab=context.getSharedPreferences("gameday-lab",Context.MODE_PRIVATE);
        SharedPreferences saved=context.getSharedPreferences("gameday-monitor",Context.MODE_PRIVATE);
        if(!lab.getBoolean("resumeEnabled",false)||!saved.getBoolean("resumeArmed",false))return;
        // Sound remains an explicit per-session opt-in after every reboot.
        context.getSharedPreferences("gameday-audio",Context.MODE_PRIVATE).edit().putBoolean("enabled",false).apply();
        Intent start=new Intent(context,GameDayService.class).setAction(GameDayService.START)
            .putExtra("gameId",saved.getLong("gameId",0)).putExtra("away",saved.getString("away",""))
            .putExtra("home",saved.getString("home","")) .putExtra("wledIp",saved.getString("wledIp",""))
            .putExtra("celebrationMode",saved.getInt("celebrationMode",0)).putExtra("delaySec",saved.getInt("delaySec",0));
        try {if(Build.VERSION.SDK_INT>=26)context.startForegroundService(start);else context.startService(start);}
        catch(RuntimeException blocked){saved.edit().putString("mode","RESUME BLOCKED").putString("detail","Open GameDay to resume; OS blocked background startup").apply();}
    }
}
