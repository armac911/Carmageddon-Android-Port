package com.dethrace.android;
import android.content.*;
import android.os.*;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.zip.*;

/** Exports performance metadata only; never game files, saves or device identifiers. */
final class Diagnostics {
    private final Context context;
    private final File directory;
    private ScheduledExecutorService sampler;
    private int samples;
    private Diagnostics(Context c,File d) {context=c;directory=d;}
    static Diagnostics start(Context c) throws Exception {
        android.system.Os.setenv("DETHRACE_PROFILE_PATH","",true);
        if(!c.getSharedPreferences("graphics",0).getBoolean("profiling",false)) return null;
        File dir=new File(new File(c.getFilesDir(),"profiles"),new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",Locale.US).format(new Date())+"-"+Long.toHexString(SystemClock.elapsedRealtime()));
        if(!dir.mkdirs()) throw new IOException("Cannot create profiling directory");
        JSONObject m=new JSONObject();
        m.put("app_version",c.getPackageManager().getPackageInfo(c.getPackageName(),0).versionName);
        m.put("game",GameContent.label(GameContent.normalize(c.getSharedPreferences("game",0).getString("edition",GameContent.ORIGINAL))));
        m.put("device",Build.MANUFACTURER+" "+Build.MODEL);m.put("android",Build.VERSION.RELEASE);
        m.put("api",Build.VERSION.SDK_INT);m.put("hardware",Build.HARDWARE);
        java.text.SimpleDateFormat utc=new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss'Z'",Locale.US);utc.setTimeZone(TimeZone.getTimeZone("UTC"));
        m.put("started_utc",utc.format(new Date()));m.put("started_elapsed_ms",SystemClock.elapsedRealtime());
        m.put("aspect",c.getSharedPreferences("display",0).getBoolean("widescreen_16_9",false)?"16:9":"4:3");
        String[] keys={"quality","distance","steering","monitor","fps_cap","filtering"};
        String[] defaults={"standard","original","analog","off","60","sharp"};
        for(int i=0;i<keys.length;i++) m.put(keys[i],c.getSharedPreferences("graphics",0).getString(keys[i],defaults[i]));
        JSONObject inputTuning=new JSONObject();
        for(int i=0;i<InputTuning.KEYS.length;i++)inputTuning.put(InputTuning.KEYS[i],
            InputTuning.clamp(i,c.getSharedPreferences("input_tuning",0).getInt(InputTuning.KEYS[i],InputTuning.DEFAULTS[i])));
        m.put("input_tuning",inputTuning);
        m.put("display_width",c.getResources().getDisplayMetrics().widthPixels);m.put("display_height",c.getResources().getDisplayMetrics().heightPixels);
        m.put("timing_notes","CPU wall-clock ms. Render includes readback/copy/presentation/swap; do not sum nested fields. Readback includes queued GPU work and transfer. No forced GPU sync or GPU timer queries. Draw calls/triangles count model geometry only. Race-loop frames only; exclude rows with excluded=1. Capture stops after 120 seconds of non-excluded race frame time. Sensors stop after 10 minutes. Battery temperature is not CPU/GPU temperature; thermal status -1 means unavailable.");
        try(FileOutputStream out=new FileOutputStream(new File(dir,"metadata.json"))) {out.write(m.toString(2).getBytes(StandardCharsets.UTF_8));}
        try(FileOutputStream out=new FileOutputStream(new File(dir,"sensors.csv"))) {out.write("elapsed_realtime_ms,battery_c,battery_percent,plugged,thermal_status,power_save,event\n".getBytes(StandardCharsets.UTF_8));}
        android.system.Os.setenv("DETHRACE_PROFILE_PATH",new File(dir,"frames.csv").getAbsolutePath(),true);
        return new Diagnostics(c,dir);
    }
    private synchronized void sample(String event) {
        try {
            Intent b=context.registerReceiver(null,new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            int temp=b==null?Integer.MIN_VALUE:b.getIntExtra(BatteryManager.EXTRA_TEMPERATURE,Integer.MIN_VALUE);
            int level=b==null?-1:b.getIntExtra(BatteryManager.EXTRA_LEVEL,-1),scale=b==null?0:b.getIntExtra(BatteryManager.EXTRA_SCALE,0);
            PowerManager pm=(PowerManager)context.getSystemService(Context.POWER_SERVICE);
            String row=String.format(Locale.US,"%d,%s,%.1f,%d,%d,%s,%s\n",SystemClock.elapsedRealtime(),temp==Integer.MIN_VALUE?"":String.format(Locale.US,"%.1f",temp/10.0),level>=0&&scale>0?100.0*level/scale:-1.0,b==null?-1:b.getIntExtra(BatteryManager.EXTRA_PLUGGED,-1),Build.VERSION.SDK_INT>=29?pm.getCurrentThermalStatus():-1,pm.isPowerSaveMode(),event);
            try(FileOutputStream out=new FileOutputStream(new File(directory,"sensors.csv"),true)) {out.write(row.getBytes(StandardCharsets.UTF_8));}
        }catch(Exception e) {android.util.Log.w("CarmaProfile","Sensor sample failed",e);}
    }
    synchronized void resume() {
        if(sampler!=null||samples>=300)return;
        sampler=Executors.newSingleThreadScheduledExecutor();
        sampler.scheduleAtFixedRate(()->{if(samples++<300)sample("active");},0,2,TimeUnit.SECONDS);
    }
    synchronized void pause() {if(sampler!=null){sampler.shutdownNow();sampler=null;}sample("paused");}
    static File[] sessions(Context c) {
        File[] dirs=new File(c.getFilesDir(),"profiles").listFiles(File::isDirectory);
        if(dirs==null)return new File[0];
        Arrays.sort(dirs,Comparator.comparing(File::getName).reversed());
        return Arrays.copyOf(dirs,Math.min(dirs.length,12));
    }
    static void export(Context c,android.net.Uri uri,File[] sessions)throws IOException {
        OutputStream stream=c.getContentResolver().openOutputStream(uri,"wt");
        if(stream==null)throw new IOException("Cannot open export destination");
        try(ZipOutputStream zip=new ZipOutputStream(new BufferedOutputStream(stream))){
            byte[] buffer=new byte[32768];
            for(File dir:sessions)for(String name:new String[]{"metadata.json","sensors.csv","frames.csv"}){
                File file=new File(dir,name);if(!file.isFile())continue;
                zip.putNextEntry(new ZipEntry(dir.getName()+"/"+name));
                try(FileInputStream in=new FileInputStream(file)){int n;while((n=in.read(buffer))!=-1)zip.write(buffer,0,n);}
                zip.closeEntry();
            }
        }
    }
}
