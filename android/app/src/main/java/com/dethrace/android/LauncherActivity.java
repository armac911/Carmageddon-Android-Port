package com.dethrace.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.*;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;

/** Launcher-only presentation. Existing game preferences and data paths are preserved. */
public class LauncherActivity extends Activity {
    private static final int PICK_ZIP=1, EXPORT_PROFILE=2, EXPORT_ERRORS=3;
    private static final int BG=0xff0c0e12, PANEL=0xff171a20, LINE=0xff30343d,
        TEXT=0xfff3f3f5, MUTED=0xffa4a9b4, RED=0xffed343f;
    private File[] exportSessions;
    private Button play, importButton;
    private TextView status;
    private File root;
    private boolean busy;
    private String page="Home";
    private LinearLayout body, nav;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        // Make diagnostic capture opt-in for both fresh installs and this upgrade.
        SharedPreferences diagnosticsPrefs=getSharedPreferences("graphics",0);
        if(!diagnosticsPrefs.getBoolean("diagnostics_opt_in_v17",false))
            diagnosticsPrefs.edit().putBoolean("profiling",false).putString("monitor","off").putBoolean("diagnostics_opt_in_v17",true).apply();
        root=getExternalFilesDir(null);
        if(saved!=null) page=saved.getString("launcher_page","Home");
        getWindow().setNavigationBarColor(BG);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);
        LinearLayout shell=new LinearLayout(this); shell.setOrientation(LinearLayout.HORIZONTAL);
        shell.setBackgroundColor(BG);
        shell.setOnApplyWindowInsetsListener((v,insets)->{
            int left=insets.getSystemWindowInsetLeft(),top=insets.getSystemWindowInsetTop();
            int right=insets.getSystemWindowInsetRight(),bottom=insets.getSystemWindowInsetBottom();
            if(android.os.Build.VERSION.SDK_INT>=28 && insets.getDisplayCutout()!=null){
                android.view.DisplayCutout cutout=insets.getDisplayCutout();
                left=Math.max(left,cutout.getSafeInsetLeft());right=Math.max(right,cutout.getSafeInsetRight());
                top=Math.max(top,cutout.getSafeInsetTop());bottom=Math.max(bottom,cutout.getSafeInsetBottom());
            }
            v.setPadding(left,top,right,bottom);
            return insets;
        });
        LinearLayout rail=new LinearLayout(this);rail.setOrientation(LinearLayout.VERTICAL);
        rail.setPadding(dp(16),dp(18),dp(16),dp(14));rail.setBackgroundColor(0xff111318);
        TextView mark=text("CARMA",25,TEXT,true); mark.setLetterSpacing(.1f);rail.addView(mark);
        TextView sub=text("ANDROID  /  0.8",10,MUTED,true);sub.setLetterSpacing(.1f);rail.addView(sub);
        nav=new LinearLayout(this);nav.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams np=new LinearLayout.LayoutParams(-1,0,1);np.topMargin=dp(18);
        // Scroll the navigation on compact screens and with larger system fonts.
        ScrollView navigationScroll=new ScrollView(this);navigationScroll.setFillViewport(false);
        navigationScroll.addView(nav);rail.addView(navigationScroll,np);
        LinearLayout brand=new LinearLayout(this);brand.setOrientation(LinearLayout.VERTICAL);
        brand.setMinimumHeight(dp(56));brand.setGravity(Gravity.CENTER_VERTICAL);
        brand.setFocusable(true);brand.setContentDescription("Android port by AndroGaming.com. Open website.");
        brand.setOnClickListener(v->openWebsite());
        TextView credit=text("ANDROID PORT BY",9,MUTED,true);credit.setLetterSpacing(.12f);brand.addView(credit);
        ImageView logo=new ImageView(this);logo.setImageResource(R.drawable.androgaming_logo);
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        logo.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        brand.addView(logo,new LinearLayout.LayoutParams(-1,dp(42)));rail.addView(brand);
        shell.addView(rail,new LinearLayout.LayoutParams(dp(180),-1));
        View divider=new View(this);divider.setBackgroundColor(LINE);shell.addView(divider,new LinearLayout.LayoutParams(dp(1),-1));
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);
        body=new LinearLayout(this);body.setOrientation(LinearLayout.VERTICAL);body.setPadding(dp(26),dp(20),dp(26),dp(22));
        scroll.addView(body);shell.addView(scroll,new LinearLayout.LayoutParams(0,-1,1));
        setContentView(shell);shell.requestApplyInsets();showPage(page);
    }
    @Override protected void onSaveInstanceState(Bundle state){super.onSaveInstanceState(state);state.putString("launcher_page",page);}
    private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
    private TextView text(String value,int size,int color,boolean bold){
        TextView v=new TextView(this);v.setText(value);v.setTextSize(size);v.setTextColor(color);
        if(bold)v.setTypeface(Typeface.create("sans-serif",Typeface.BOLD));return v;
    }
    private GradientDrawable surface(int color,int radius){
        GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(radius));d.setStroke(dp(1),LINE);return d;
    }
    private Button button(String label,boolean primary){
        Button b=new Button(this);b.setText(label);b.setAllCaps(false);b.setTextSize(15);
        b.setTypeface(Typeface.DEFAULT,Typeface.BOLD);b.setGravity(Gravity.CENTER);
        b.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{0xff747984,TEXT}));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x44ffffff),surface(primary?RED:0xff22262e,10),null));
        b.setMinHeight(dp(48));b.setPadding(dp(12),dp(9),dp(12),dp(9));
        LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.topMargin=dp(10);b.setLayoutParams(lp);return b;
    }
    private LinearLayout card(){
        LinearLayout c=new LinearLayout(this);c.setOrientation(LinearLayout.VERTICAL);c.setPadding(dp(18),dp(14),dp(18),dp(16));
        c.setBackground(surface(PANEL,14));LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,-2);lp.topMargin=dp(14);c.setLayoutParams(lp);body.addView(c);return c;
    }
    private void heading(String title,String description){
        body.addView(text(title,28,TEXT,true));TextView d=text(description,13,MUTED,false);d.setPadding(0,dp(5),0,dp(3));body.addView(d);
    }
    private void note(LinearLayout c,String value){TextView t=text(value,12,MUTED,false);t.setPadding(0,dp(8),0,dp(2));c.addView(t);}
    private void showPage(String name){
        page=name;body.removeAllViews();nav.removeAllViews();play=null;importButton=null;status=null;
        String[] pages={"Home","Graphics","Controls","Diagnostics"};
        for(String n:pages){
            Button b=button(n,false);b.setTextSize(14);b.setGravity(Gravity.CENTER_VERTICAL|Gravity.LEFT);
            b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33ffffff),surface(n.equals(page)?0xff402027:0xff111318,8),null));
            b.setTextColor(n.equals(page)?0xffff626b:MUTED);b.setOnClickListener(v->showPage(n));
            LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(-1,dp(48));lp.bottomMargin=dp(6);nav.addView(b,lp);
        }
        if("Graphics".equals(page))graphics();else if("Controls".equals(page))controls();
        else if("Diagnostics".equals(page))diagnostics();else home();
    }
    private void openWebsite(){
        try{startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse("https://androgaming.com")));}
        catch(android.content.ActivityNotFoundException e){Toast.makeText(this,"No web browser is available.",Toast.LENGTH_SHORT).show();}
    }
    private String pref(String key,String fallback){return getSharedPreferences("graphics",0).getString(key,fallback);}
    private void home(){
        TextView tag=text("CLASSIC CHAOS. BACK ON THE ROAD.",10,RED,true);tag.setLetterSpacing(.12f);body.addView(tag);
        TextView title=text("CARMAGEDDON",34,TEXT,true);title.setTypeface(Typeface.create("sans-serif-condensed",Typeface.BOLD));
        title.setPadding(0,dp(8),0,0);body.addView(title);
        body.addView(text("Your next race starts here.",14,MUTED,false));
        LinearLayout launch=card();
        status=text("",14,TEXT,true);launch.addView(status);
        String cap=pref("fps_cap","60");String quality=pref("quality","standard");String distance=pref("distance","original");
        String aspect=getSharedPreferences("display",0).getBoolean("widescreen_16_9",false)?"16:9":"4:3";
        note(launch,aspect+"  •  "+capitalize(quality)+"  •  "+capitalize(distance)+" distance  •  "+("0".equals(cap)?"Uncapped":cap+" FPS"));
        play=button("PLAY  ›",true);play.setTextSize(20);play.setOnClickListener(v->startActivity(new Intent(this,DethraceActivity.class)));launch.addView(play);
        importButton=button("Import game data ZIP",false);importButton.setOnClickListener(v->pickData());launch.addView(importButton);
        note(launch,"Use your own PC game files: DATA/ at the ZIP root, with optional MUSIC/. Game content is not included.");
        TextView about=text("Powered by Dethrace  •  Version 0.8",11,MUTED,false);about.setPadding(0,dp(15),0,0);body.addView(about);
        update();
    }
    private String capitalize(String s){return s.isEmpty()?s:Character.toUpperCase(s.charAt(0))+s.substring(1);}
    private void graphics(){
        heading("Graphics","Set up your next run. Changes apply when you press Play.");
        LinearLayout c=card();
        choice(c,"Aspect ratio","display","widescreen_16_9",new String[]{"4:3 — Classic","16:9 — Widescreen"},new String[]{"false","true"},true);
        choice(c,"3D resolution","graphics","quality",new String[]{"Standard","High — sharper edges","Performance — lower resolution"},new String[]{"standard","high","performance"},false);
        choice(c,"3D texture filtering","graphics","filtering",new String[]{"Sharp — original textures","Smooth — bilinear textures"},new String[]{"sharp","smooth"},false);
        note(c,"Smooth blends 3D texture colors within the game palette. Menus and HUD stay sharp. Changes apply on the next Play.");
        choice(c,"Draw distance","graphics","distance",new String[]{"Original","Extended (1.5×)","Far (2×)"},new String[]{"original","extended","far"},false);
        choice(c,"Frame-rate cap","graphics","fps_cap",new String[]{"60 FPS","120 FPS","144 FPS","Uncapped"},new String[]{"60","120","144","0"},false);
        note(c,"The cap covers menus and races. Higher settings may reduce performance. A higher cap does not change your display’s refresh rate.");
    }
    private void controls(){
        heading("Controls","Make it feel right.");LinearLayout c=card();
        choice(c,"Steering · touch & left stick","graphics","steering",new String[]{"Analog","Digital"},new String[]{"analog","digital"},false);
        toggle(c,"Haptic feedback","controls","haptics",true);
        Button customize=button("Customize touch layout  ›",false);customize.setOnClickListener(v->startActivity(new Intent(this,ControlEditorActivity.class)));c.addView(customize);
        Button controller=button("Remap controller  ›",false);controller.setOnClickListener(v->controllerMapping());c.addView(controller);
        note(c,"Defaults: left stick / D-pad steer · RT or X accelerate · LT brake/reverse · LB handbrake · Y recover · RB repair (hold) · Select map · right stick up/down gears · R3 camera · Start pause. Menus: A confirm, B back.");
        note(c,"Move and resize touch controls in the layout editor.");
    }
    private void controllerMapping(){
        body.removeAllViews();heading("Controller mapping","Changes apply on the next Play. Return the gear stick to centre between shifts.");
        LinearLayout c=card();
        choice(c,"Steering input","controller","steering",new String[]{"Left stick + D-pad","Left stick only","Right stick only","D-pad only"},new String[]{"0","1","2","3"},false);
        for(int i=0;i<ControllerBindings.ACTIONS.length;i++){
            final int action=i;
            SharedPreferences prefs=getSharedPreferences("controller",0);
            String current=prefs.getString("bind_"+i,ControllerBindings.DEFAULTS[i]);
            int selected=0;for(int n=0;n<ControllerBindings.VALUES.length;n++)if(ControllerBindings.VALUES[n].equals(current))selected=n;
            Button row=button(ControllerBindings.ACTIONS[i]+"\n"+ControllerBindings.LABELS[selected]+"  ›",false);row.setGravity(Gravity.LEFT|Gravity.CENTER_VERTICAL);row.setTextSize(14);
            row.setOnClickListener(v->{
                String now=prefs.getString("bind_"+action,ControllerBindings.DEFAULTS[action]);int checked=0;
                for(int n=0;n<ControllerBindings.VALUES.length;n++)if(ControllerBindings.VALUES[n].equals(now))checked=n;
                new AlertDialog.Builder(this).setTitle(ControllerBindings.ACTIONS[action]).setSingleChoiceItems(ControllerBindings.LABELS,checked,(dialog,index)->{
                    prefs.edit().putString("bind_"+action,ControllerBindings.VALUES[index]).apply();
                    row.setText(ControllerBindings.ACTIONS[action]+"\n"+ControllerBindings.LABELS[index]+"  ›");dialog.dismiss();
                }).setNegativeButton("Cancel",null).show();
            });c.addView(row);
        }
        note(c,"Shared inputs trigger every assigned action. Avoid assigning gears or camera to your steering axis. Automatic shifting remains active after manual gear overrides.");
        Button reset=button("Restore controller defaults",false);reset.setOnClickListener(v->{getSharedPreferences("controller",0).edit().clear().apply();controllerMapping();});c.addView(reset);
        Button back=button("Back to Controls",false);back.setOnClickListener(v->showPage("Controls"));c.addView(back);
    }
    private void diagnostics(){
        heading("Diagnostics","Optional tools for testing and troubleshooting.");LinearLayout c=card();
        choice(c,"In-game performance overlay","graphics","monitor",new String[]{"Off","FPS","Battery temperature","FPS + battery temperature"},new String[]{"off","fps","temperature","both"},false);
        toggle(c,"Record performance diagnostics","graphics","profiling",false);
        note(c,"Records up to 120 seconds of racing per session. For comparisons: stay stationary for 20 seconds, then drive for 30 seconds.");
        Button export=button("Export diagnostics ZIP  ↑",false);export.setOnClickListener(v->exportDiagnostics());c.addView(export);
        note(c,"Includes the latest 12 sessions, graphics settings and battery/thermal readings. No game data. Nothing is uploaded automatically.");
        LinearLayout errors=card();
        Switch errorSwitch=new Switch(this);errorSwitch.setText("Record error logs");errorSwitch.setTextColor(TEXT);errorSwitch.setTextSize(14);errorSwitch.setMinHeight(dp(52));
        errorSwitch.setChecked(getSharedPreferences("diagnostics",0).getBoolean("error_logging",false));
        errorSwitch.setOnCheckedChangeListener((button,checked)->ErrorLogs.setEnabled(this,checked));errors.addView(errorSwitch);
        note(errors,"Off by default. Enable before Play. After an unexpected exit, return here and export the error ZIP. Recording is separate from performance diagnostics.");
        Button errorExport=button("Export error logs ZIP  ↑",false);errorExport.setOnClickListener(v->exportErrors());errors.addView(errorExport);
        note(errors,"Keeps up to 12 bounded sessions: app errors, lifecycle events and available Android exit reports. Reports vary by Android/device. Logs may contain app messages and file paths. Nothing is uploaded automatically.");
    }
    private void choice(LinearLayout c,String title,String store,String key,String[] labels,String[] values,boolean bool){
        SharedPreferences prefs=getSharedPreferences(store,0);
        String current=bool?String.valueOf(prefs.getBoolean(key,false)):prefs.getString(key,values[0]);
        int selected=0;for(int i=0;i<values.length;i++)if(values[i].equals(current))selected=i;
        final int initial=selected;
        Button row=button(title+"\n"+labels[selected]+"  ›",false);row.setGravity(Gravity.LEFT|Gravity.CENTER_VERTICAL);row.setTextSize(14);
        row.setOnClickListener(v->{
            String now=bool?String.valueOf(prefs.getBoolean(key,false)):prefs.getString(key,values[0]);
            int checked=initial;for(int i=0;i<values.length;i++)if(values[i].equals(now))checked=i;
            new AlertDialog.Builder(this).setTitle(title).setSingleChoiceItems(labels,checked,(dialog,index)->{
                if(bool)prefs.edit().putBoolean(key,Boolean.parseBoolean(values[index])).apply();else prefs.edit().putString(key,values[index]).apply();
                row.setText(title+"\n"+labels[index]+"  ›");dialog.dismiss();
            }).setNegativeButton("Cancel",null).show();
        });c.addView(row);
    }
    private void toggle(LinearLayout c,String title,String store,String key,boolean fallback){
        Switch v=new Switch(this);v.setText(title);v.setTextColor(TEXT);v.setTextSize(14);v.setPadding(0,dp(12),0,dp(12));v.setMinHeight(dp(52));
        v.setChecked(getSharedPreferences(store,0).getBoolean(key,fallback));v.setOnCheckedChangeListener((button,checked)->getSharedPreferences(store,0).edit().putBoolean(key,checked).apply());c.addView(v);
    }
    private void pickData(){
        Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES,new String[]{"application/zip","application/x-zip-compressed","application/octet-stream"});startActivityForResult(intent,PICK_ZIP);
    }
    private void exportErrors(){
        if(ErrorLogStore.sessions(ErrorLogs.root(this)).length==0){new AlertDialog.Builder(this).setMessage("No error sessions yet. Enable Record error logs, then press Play. After an unexpected exit, reopen the launcher and export here.").setPositiveButton("OK",null).show();return;}
        Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("application/zip");
        intent.putExtra(Intent.EXTRA_TITLE,"carma-errors-"+new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",java.util.Locale.US).format(new java.util.Date())+".zip");startActivityForResult(intent,EXPORT_ERRORS);
    }
    private void exportDiagnostics(){
        exportSessions=Diagnostics.sessions(this);
        if(exportSessions.length==0){new AlertDialog.Builder(this).setMessage("No profiling sessions yet. Enable recording and start a race first.").setPositiveButton("OK",null).show();return;}
        Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("application/zip");
        intent.putExtra(Intent.EXTRA_TITLE,"carma-profiling-"+new java.text.SimpleDateFormat("yyyyMMdd-HHmmss",java.util.Locale.US).format(new java.util.Date())+".zip");startActivityForResult(intent,EXPORT_PROFILE);
    }
    @Override protected void onResume(){super.onResume();update();ErrorLogs.recoverAsync(this);}
    @Override public void onBackPressed(){if(!"Home".equals(page))showPage("Home");else super.onBackPressed();}
    private void update(){
        if(play==null||importButton==null||status==null)return;
        boolean ready=root!=null&&GameDataZip.isReady(new File(root,"game"));
        play.setEnabled(ready&&!busy);play.setAlpha(ready&&!busy?1f:.45f);
        importButton.setEnabled(root!=null&&!busy&&!ready);importButton.setVisibility(ready?View.GONE:View.VISIBLE);
        status.setText(busy?"Importing game data…":ready?"●  Ready to race":root==null?"Storage unavailable":"Import your game data to begin");
        status.setTextColor(ready?0xff72d3aa:MUTED);
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if(request==EXPORT_ERRORS&&result==RESULT_OK&&data!=null&&data.getData()!=null){
            final Uri destination=data.getData();
            new Thread(()->{
                String message="Error logs exported. Share the ZIP for investigation.";
                try{ErrorLogs.export(this,destination);}catch(Exception e){message="Error export failed: "+e.getMessage();}
                final String text=message;
                runOnUiThread(()->{if(!isFinishing()&&!isDestroyed())new AlertDialog.Builder(this).setMessage(text).setPositiveButton("OK",null).show();});
            },"error-export").start();return;
        }
        if (request == EXPORT_PROFILE && result == RESULT_OK && data != null && data.getData() != null) {
            final Uri destination=data.getData();
            final File[] sessions=exportSessions!=null?exportSessions:Diagnostics.sessions(this);
            new Thread(() -> {
                String message="Diagnostics exported. Send the ZIP for analysis.";
                try { Diagnostics.export(this,destination,sessions); } catch(Exception e) { message="Export failed: "+e.getMessage(); }
                final String text=message;
                runOnUiThread(() -> { if(!isFinishing()&&!isDestroyed()) new AlertDialog.Builder(this).setMessage(text).setPositiveButton("OK",null).show(); });
            },"profile-export").start();
            return;
        }
        if (request != PICK_ZIP || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        busy = true;
        update();
        new Thread(() -> {
            File staging = new File(root, "import-staging");
            String failure = null;
            try {
                GameDataZip.deleteTree(staging);
                try (InputStream stream = getContentResolver().openInputStream(uri)) {
                    if (stream == null) throw new IOException("Cannot open ZIP");
                    GameDataZip.extract(stream, staging);
                }
                File game = new File(root, "game");
                if (game.exists() || !staging.renameTo(game)) throw new IOException("Cannot install data directory");
            } catch (Exception e) {
                failure = e.getMessage();
                try { GameDataZip.deleteTree(staging); } catch (IOException ignored) {}
            }
            final String message = failure;
            runOnUiThread(() -> {
                busy = false;
                if (isFinishing() || isDestroyed()) return;
                update();
                if (message != null) new AlertDialog.Builder(this).setTitle("Import failed")
                    .setMessage(message).setPositiveButton("OK", null).show();
            });
        }, "game-data-import").start();
    }
}
