package com.dethrace.android;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Color;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;

/** Edits a scaled preview with the same aspect and geometry as the game surface. */
public class ControlEditorActivity extends Activity {
    private TouchOverlayView preview;
    private TextView sizeLabel, hint;
    private SeekBar size;
    private int dp(int n) { return Math.round(n*getResources().getDisplayMetrics().density); }
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        UiLanguage.set(getSharedPreferences("language",0).getString("code","en"));
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (android.os.Build.VERSION.SDK_INT>=28) getWindow().getAttributes().layoutInDisplayCutoutMode=
            android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        LinearLayout root=new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL); root.setBackgroundColor(Color.rgb(23,29,36));
        LinearLayout tools=new LinearLayout(this); tools.setGravity(Gravity.CENTER_VERTICAL);
        tools.setPadding(dp(16),0,dp(16),0);
        sizeLabel=new TextView(this);sizeLabel.setTextColor(Color.WHITE);sizeLabel.setTextSize(16);
        tools.addView(sizeLabel,new LinearLayout.LayoutParams(dp(95),dp(48)));
        size=new SeekBar(this);size.setMax(70);
        tools.addView(size,new LinearLayout.LayoutParams(0,dp(48),1));
        root.addView(tools);
        LinearLayout actions=new LinearLayout(this);actions.setGravity(Gravity.CENTER);
        button(actions,"Standard",()->preview.setPreset(false));
        button(actions,"Mirrored",()->preview.setPreset(true));
        button(actions,"Reset",()->{preview.restoreDefaults();size.setProgress(30);});
        button(actions,"Cancel",this::finish);
        button(actions,"Save",()->{
            if(preview.saveLayout()) {Toast.makeText(this,UiLanguage.text("Controls saved"),Toast.LENGTH_SHORT).show();finish();}
            else hint.setText(UiLanguage.text(preview.getEditMessage()));
        });
        root.addView(actions);
        hint=new TextView(this);hint.setTextColor(Color.LTGRAY);hint.setTextSize(12);hint.setGravity(Gravity.CENTER);
        hint.setText(UiLanguage.text("Drag controls to move. Tap an icon for its name. Recovery needs a 0.6 second hold."));
        root.addView(hint);
        FrameLayout area=new FrameLayout(this);
        root.addView(area,new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,0,1));
        preview=new TouchOverlayView(this,new TouchOverlayView.Listener(){
            public void input(int mask,int axis,boolean active) {}
            public void reset() {}
        });
        preview.setEditing(true);preview.setBackgroundColor(Color.rgb(40,49,60));
        area.addView(preview,new FrameLayout.LayoutParams(1,1,Gravity.CENTER));
        // The preview is scaled uniformly; saved centers use the full game's normalized coordinates.
        area.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->{
            int ww=getWindow().getDecorView().getWidth(), wh=getWindow().getDecorView().getHeight();
            if(ww<=0||wh<=0)return;
            float aspect=(float)ww/wh;
            int height=Math.min(b-t,Math.round((r-l)/aspect));
            int width=Math.round(height*aspect);
            preview.setLayoutParams(new FrameLayout.LayoutParams(width,height,Gravity.CENTER));
        });
        preview.setOnTouchListener((v,event)->{v.post(()->hint.setText(UiLanguage.text(preview.getEditMessage())));return false;});
        size.setProgress(preview.getSizePercent()-70);
        sizeLabel.setText(UiLanguage.text("Size: "+preview.getSizePercent()+"%"));
        size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
            public void onProgressChanged(SeekBar s,int value,boolean user){
                preview.setSizePercent(value+70);sizeLabel.setText(UiLanguage.text("Size: "+(value+70)+"%"));
            }
            public void onStartTrackingTouch(SeekBar s){}
            public void onStopTrackingTouch(SeekBar s){}
        });
        setContentView(root);
    }
    private void button(LinearLayout row,String label,Runnable action){
        Button button=new Button(this);button.setText(UiLanguage.text(label));button.setTextSize(12);button.setAllCaps(false);
        button.setOnClickListener(v->{action.run();hint.setText(UiLanguage.text(preview.getEditMessage()));});
        row.addView(button,new LinearLayout.LayoutParams(0,dp(44),1));
    }
}
