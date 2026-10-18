package com.dethrace.android;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import org.json.JSONObject;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** No collection until explicitly enabled; all data stays in app-private files. */
final class ErrorLogs {
    private static volatile boolean enabled;
    private static boolean installed;
    private static volatile File active;
    private static java.lang.Process logcat;
    private static Thread reader;
    private static volatile boolean recovering;
    static File root(Context c){return new File(c.getFilesDir(),"errors");}
    static synchronized void install(Context c){
        enabled=c.getSharedPreferences("diagnostics",0).getBoolean("error_logging",false);
        if(installed)return;installed=true;
        final Thread.UncaughtExceptionHandler previous=Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new ErrorExceptionHandler(
            ()->enabled?active:null,previous,()->{
                android.os.Process.killProcess(android.os.Process.myPid());System.exit(10);
            }));
    }

    static synchronized void setEnabled(Context c,boolean on){
        c.getSharedPreferences("diagnostics",0).edit().putBoolean("error_logging",on).apply();
        enabled=on;
        if(!on){finishGame();try{android.system.Os.setenv("DETHRACE_ERROR_DIRECTORY","",true);}catch(Exception ignored){}}
    }
    static synchronized void startGame(Context c){
        finishGame();
        try{android.system.Os.setenv("DETHRACE_ERROR_DIRECTORY","",true);}catch(Exception ignored){}
        if(!enabled)return;
        try {
            File dir=new File(root(c),new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",Locale.US).format(new Date())+"-"+SystemClock.elapsedRealtime());
            if(!dir.mkdirs())throw new IOException("Cannot create error session");
            JSONObject m=new JSONObject();
            m.put("app_version",c.getPackageManager().getPackageInfo(c.getPackageName(),0).versionName);
        m.put("game",GameContent.label(GameContent.normalize(c.getSharedPreferences("game",0).getString("edition",GameContent.ORIGINAL))));
            m.put("version_code",c.getPackageManager().getPackageInfo(c.getPackageName(),0).versionCode);
            m.put("device",Build.MANUFACTURER+" "+Build.MODEL);m.put("android",Build.VERSION.RELEASE);m.put("api",Build.VERSION.SDK_INT);
            m.put("error_logging",true);m.put("pid",android.os.Process.myPid());m.put("started_utc_ms",System.currentTimeMillis());m.put("started_elapsed_ms",SystemClock.elapsedRealtime());
            m.put("graphics",new JSONObject(c.getSharedPreferences("graphics",0).getAll()));
            m.put("controller_bindings",ControllerBindings.serialize(c.getSharedPreferences("controller",0)));
            m.put("aspect",c.getSharedPreferences("display",0).getBoolean("widescreen_16_9",false)?"16:9":"4:3");
            m.put("limits","Engine 2 MiB x2; logcat 1 MiB x2; Java exception 256 KiB; at most two 1 MiB system traces per session; latest 12 sessions retained. Traces are best effort, not guaranteed.");
            writeJson(new File(dir,"metadata.json"),m);active=dir;event("game.start");
            android.system.Os.setenv("DETHRACE_ERROR_DIRECTORY",dir.getAbsolutePath(),true);
            startLogcat(dir);ErrorLogStore.prune(root(c));
        }catch(Exception e){event("Error capture setup failed: "+e);android.util.Log.w("CarmaErrors","Error capture unavailable",e);}
    }
    private static void startLogcat(File dir){
        try {
            final java.lang.Process process=new ProcessBuilder("/system/bin/logcat","-v","threadtime","--pid="+android.os.Process.myPid(),"-b","main","-b","system","-b","crash","-T",new java.text.SimpleDateFormat("MM-dd HH:mm:ss.SSS",Locale.US).format(new Date()),"*:W").redirectErrorStream(true).start();
            logcat=process;
            reader=new Thread(()->{
                try(BufferedReader input=new BufferedReader(new InputStreamReader(process.getInputStream(),StandardCharsets.UTF_8))){
                    String line;while((line=input.readLine())!=null){
                        synchronized(ErrorLogs.class){if(!enabled||active!=dir||logcat!=process)break;
                            ErrorLogStore.append(new File(dir,"logcat.log"),line+"\n",ErrorLogStore.LOG_BYTES);}
                    }
                }catch(IOException e){synchronized(ErrorLogs.class){if(enabled&&active==dir)event("App log stream unavailable: "+e);}}
                finally{process.destroy();}
            },"CarmaErrorLogcat");reader.setDaemon(true);reader.start();
        }catch(Exception e){event("App logcat unavailable: "+e);}
    }
    static synchronized void event(String text){
        File dir=active;if(!enabled||dir==null)return;
        try{ErrorLogStore.append(new File(dir,"lifecycle.log"),System.currentTimeMillis()+" "+SystemClock.elapsedRealtime()+" "+text+"\n",256*1024);}catch(Exception ignored){}
    }
    static synchronized void finishGame(){
        File dir=active;
        if(dir!=null){event("game.activity_destroyed_or_session_closed (not a crash diagnosis)");
            try{JSONObject m=readJson(new File(dir,"metadata.json"));m.put("ended_utc_ms",System.currentTimeMillis());writeJson(new File(dir,"metadata.json"),m);}catch(Exception ignored){}
        }
        active=null;
        if(logcat!=null){logcat.destroy();logcat=null;}reader=null;
    }
    static void recoverAsync(Context c){
        if(!enabled||recovering)return;recovering=true;
        Context app=c.getApplicationContext();
        new Thread(()->{try{recover(app);}finally{recovering=false;}},"CarmaErrorRecovery").start();
    }
    private static synchronized void recover(Context c){
        if(Build.VERSION.SDK_INT<30)return;
        try{recoverApi30(c);}catch(Exception e){android.util.Log.w("CarmaErrors","Exit history unavailable",e);}
    }
    @android.annotation.TargetApi(30)
    private static void recoverApi30(Context c)throws Exception {
        ActivityManager manager=(ActivityManager)c.getSystemService(Context.ACTIVITY_SERVICE);
        File[] sessions=ErrorLogStore.sessions(root(c));
        if(manager==null||sessions.length==0)return;
        for(ApplicationExitInfo exit:manager.getHistoricalProcessExitReasons(c.getPackageName(),0,32)){
            File match=null;long latest=0;
            for(File dir:sessions){
                JSONObject m=readJson(new File(dir,"metadata.json"));long start=m.optLong("started_utc_ms");
                long end=m.optLong("ended_utc_ms",Long.MAX_VALUE-5000);
                if(m.optInt("pid")==exit.getPid()&&start<=exit.getTimestamp()&&start>latest&&exit.getTimestamp()<=end+5000){match=dir;latest=start;}
            }
            if(match==null)continue;
            String prefix="exit-"+exit.getTimestamp()+"-"+exit.getReason();File report=new File(match,prefix+".json");
            if(report.exists())continue;
            File[] prior=match.listFiles((d,n)->n.startsWith("exit-")&&n.endsWith(".json"));
            if(prior!=null&&prior.length>=2)continue;
            JSONObject m=new JSONObject();m.put("timestamp_utc_ms",exit.getTimestamp());m.put("reason_code",exit.getReason());
            m.put("reason",reasonName(exit.getReason()));m.put("status",exit.getStatus());m.put("description",exit.getDescription());
            m.put("importance",exit.getImportance());m.put("pss_kib",exit.getPss());m.put("rss_kib",exit.getRss());m.put("process_name",exit.getProcessName());
            String suffix=exit.getReason()==ApplicationExitInfo.REASON_CRASH_NATIVE&&Build.VERSION.SDK_INT>=31?".trace.pb":".trace.txt";
            try(InputStream input=exit.getTraceInputStream()){
                if(input!=null){try(OutputStream out=new FileOutputStream(new File(match,prefix+suffix))){long n=ErrorLogStore.copyBounded(input,out,1024*1024);m.put("trace_bytes",n);m.put("trace_limit_reached",n==1024*1024);}}
                else m.put("trace_status","Not supplied by Android");
            }catch(Exception e){m.put("trace_status","Unavailable: "+e.getClass().getSimpleName());}
            writeJson(report,m);
        }
    }
    static String reasonName(int reason){
        switch(reason){case 1:return "EXIT_SELF";case 2:return "SIGNALED";case 3:return "LOW_MEMORY";case 4:return "JAVA_CRASH";case 5:return "NATIVE_CRASH";case 6:return "ANR";case 7:return "INITIALIZATION_FAILURE";case 10:return "USER_REQUESTED";default:return "ANDROID_REASON_"+reason;}
    }
    private static JSONObject readJson(File file)throws Exception {
        try(InputStream in=new FileInputStream(file);ByteArrayOutputStream out=new ByteArrayOutputStream()){
            ErrorLogStore.copyBounded(in,out,128*1024);return new JSONObject(out.toString("UTF-8"));}
    }
    private static void writeJson(File file,JSONObject data)throws Exception {
        File tmp=new File(file.getPath()+".tmp");
        try(FileOutputStream out=new FileOutputStream(tmp)){out.write(data.toString(2).getBytes(StandardCharsets.UTF_8));out.getFD().sync();}
        if(!tmp.renameTo(file))throw new IOException("Cannot save error metadata");
    }
    static void export(Context c,android.net.Uri uri)throws IOException {
        recover(c);
        OutputStream stream=c.getContentResolver().openOutputStream(uri,"wt");
        if(stream==null)throw new IOException("Cannot open error ZIP destination");
        synchronized(ErrorLogs.class){ErrorLogStore.export(stream,ErrorLogStore.sessions(root(c)));}
    }
    private ErrorLogs(){}
}
