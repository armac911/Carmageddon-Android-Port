package com.dethrace.android;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.os.Handler;
import android.os.Looper;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

import java.io.File;

public class DethraceActivity extends SDLActivity {
    private static volatile boolean gameRunning;
    static boolean isGameRunning(){return gameRunning;}

    private static final String TAG = "DethRace";

    private Diagnostics diagnostics;
    private native void nativeProfilePause();
    private TouchOverlayView mOverlay;
    private android.widget.TextView mMonitor;
    private String monitorMode = "off";
    private native int nativeFpsTenths();
    private final Handler mUiHandler = new Handler(Looper.getMainLooper());

    private native boolean nativeIsInRace();
    private native boolean nativeUsingGamepad();
    private native void nativeScreenTouched();
    @Override public boolean dispatchTouchEvent(android.view.MotionEvent event) {
        if (event.isFromSource(android.view.InputDevice.SOURCE_TOUCHSCREEN) &&
            event.getActionMasked()==android.view.MotionEvent.ACTION_DOWN && mOverlay!=null) {
            nativeScreenTouched();
            if (nativeIsInRace() && mOverlay.getVisibility()!=View.VISIBLE) {
                mOverlay.reset();
                mOverlay.setVisibility(View.VISIBLE);
            }
        }
        return super.dispatchTouchEvent(event);
    }
    private native boolean nativeTakeImpact();
    private native void nativeTouch(int mask, int axis, boolean steering);
    private native void nativeResetTouch();
    private native void nativeSubmitText(int session, String text, boolean accept);
    private android.app.AlertDialog textDialog;

    // Called on the game thread; all Android UI work is posted to the UI thread.
    public void showGameTextInput(int session, String initial, int maxLength) {
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) return;
            closeTextDialog();
            android.widget.EditText edit = new android.widget.EditText(new android.view.ContextThemeWrapper(this, android.R.style.Theme_Material_Light_NoActionBar));
            edit.setSingleLine(true);
            float density = getResources().getDisplayMetrics().density;
            edit.setTextSize(20);
            edit.setTextColor(android.graphics.Color.rgb(20,24,28));
            edit.setBackgroundColor(android.graphics.Color.WHITE);
            edit.setGravity(android.view.Gravity.CENTER_VERTICAL);
            edit.setPadding((int)(12*density),0,(int)(12*density),0);
            edit.setMinimumHeight((int)(56*density));
            edit.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
            edit.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_DONE | android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);
            edit.setFilters(new android.text.InputFilter[] {
                (source, start, end, dest, dstart, dend) -> {
                    StringBuilder clean = new StringBuilder();
                    for (int i = start; i < end; i++) {
                        char c = source.charAt(i);
                        if (c >= 32 && c <= 126) clean.append(c);
                    }
                    return clean.toString();
                }, new android.text.InputFilter.LengthFilter(maxLength)
            });
            edit.setText(initial);
            edit.selectAll();
            int pad = (int)(20 * getResources().getDisplayMetrics().density);
            android.widget.FrameLayout box = new android.widget.FrameLayout(this);
            box.setPadding(pad,(int)(8*density),pad,(int)(8*density));
            box.addView(edit, new android.widget.FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, (int)(56*density)));
            android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(this, android.R.style.Theme_Material_Light_Dialog_Alert)
                .setView(box)
                .setPositiveButton("Done", (d,w) -> nativeSubmitText(session,edit.getText().toString(),true))
                .setNegativeButton("Cancel", (d,w) -> nativeSubmitText(session,"",false))
                .create();
            dialog.setCanceledOnTouchOutside(false);
            dialog.setOnCancelListener(d -> nativeSubmitText(session,"",false));
            dialog.setOnDismissListener(d -> {
                if (textDialog == dialog) textDialog = null;
                applyImmersiveMode();
            });
            textDialog = dialog;
            edit.setOnEditorActionListener((v,action,event) -> {
                if (action == android.view.inputmethod.EditorInfo.IME_ACTION_DONE) {
                    nativeSubmitText(session,edit.getText().toString(),true); dialog.dismiss(); return true;
                }
                return false;
            });
            dialog.setOnShowListener(d -> {
                dialog.getWindow().setGravity(android.view.Gravity.TOP | android.view.Gravity.CENTER_HORIZONTAL);
                dialog.getWindow().setLayout(Math.min(getResources().getDisplayMetrics().widthPixels-(int)(32*density), (int)(560*density)), ViewGroup.LayoutParams.WRAP_CONTENT);
                edit.requestFocus();
                dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                edit.postDelayed(() -> {
                    if (dialog.isShowing()) ((android.view.inputmethod.InputMethodManager)getSystemService(INPUT_METHOD_SERVICE))
                        .showSoftInput(edit,android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
                },150);
            });
            dialog.show();
        });
    }
    private void closeTextDialog() {
        if (textDialog != null) { textDialog.dismiss(); textDialog = null; }
    }
    public void closeGameTextInput() { runOnUiThread(this::closeTextDialog); }

    @Override protected void onPause() {
        ErrorLogs.event("game.onPause");
        if (mOverlay != null) mOverlay.reset();
        mUiHandler.removeCallbacks(mPollMonitor);
        try { nativeProfilePause(); } catch (UnsatisfiedLinkError ignored) {}
        if (diagnostics != null) diagnostics.pause();
        super.onPause();
    }


    @Override protected void onResume() {
        super.onResume();
        ErrorLogs.event("game.onResume");
        if (diagnostics != null) diagnostics.resume();
        mUiHandler.removeCallbacks(mPollMonitor);
        if (mMonitor != null) mUiHandler.postDelayed(mPollMonitor, 500);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "main"
        };
    }

    @Override
    protected String getMainSharedObject() {
        String library;
        String[] libraries = getLibraries();
        if (libraries.length > 0) {
            library = "lib" + libraries[libraries.length - 1] + ".so";
        } else {
            library = "libmain.so";
        }
        return getContext().getApplicationInfo().nativeLibraryDir + "/" + library;
    }

    @Override
    protected String[] getArguments() {
        String cap=getSharedPreferences("graphics",MODE_PRIVATE).getString("fps_cap","60");
        if (!"60".equals(cap) && !"120".equals(cap) && !"144".equals(cap) && !"0".equals(cap)) cap="60";
        return new String[] { "-hires", "--fps="+cap };
    }

    @Override
    protected void onDestroy() {
        mUiHandler.removeCallbacks(mPollRaceState);
        mUiHandler.removeCallbacks(mPollMonitor);
        closeTextDialog();
        if (diagnostics != null) diagnostics.pause();
        if (mOverlay != null) mOverlay.reset();
        try { super.onDestroy(); } finally { gameRunning=false;ErrorLogs.finishGame(); }
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }

    // SDL queries this; force landscape regardless of game window aspect.
    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        super.setOrientationBis(0, 0, resizable, "LandscapeRight LandscapeLeft");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ErrorLogs.startGame(this);
        File appDir = getExternalFilesDir(null);
        String edition=GameContent.normalize(getSharedPreferences("game",0).getString("edition",GameContent.ORIGINAL));
        File extDir = appDir != null ? GameContent.directory(appDir,edition) : null;
        String rootDir = (extDir != null) ? extDir.getAbsolutePath() : GameContent.directory(getFilesDir(),edition).getAbsolutePath();

        File f = new File(rootDir);
        if (!f.exists()) {
            f.mkdirs();
        }

        Log.i(TAG, "DETHRACE_ROOT_DIR=" + rootDir);

        try {
            android.system.Os.setenv("DETHRACE_ROOT_DIR", rootDir, true);
            boolean wide = getSharedPreferences("display", MODE_PRIVATE).getBoolean("widescreen_16_9", false);
            android.system.Os.setenv("DETHRACE_WIDESCREEN_16_9", wide ? "1" : "0", true);
            String distance = getSharedPreferences("graphics", MODE_PRIVATE).getString("distance", "original");
            String quality = getSharedPreferences("graphics", MODE_PRIVATE).getString("quality", "standard");
            String steering = getSharedPreferences("graphics", MODE_PRIVATE).getString("steering", "analog");
            android.system.Os.setenv("DETHRACE_STEERING", steering, true);
            android.content.SharedPreferences controller=getSharedPreferences("controller",MODE_PRIVATE);
            android.system.Os.setenv("DETHRACE_CONTROLLER_BINDINGS", ControllerBindings.serialize(controller), true);
            android.system.Os.setenv("DETHRACE_CONTROLLER_STEERING", controller.getString("steering","0"), true);
            android.system.Os.setenv("DETHRACE_DRAW_DISTANCE", distance, true);
            android.system.Os.setenv("DETHRACE_RENDER_QUALITY", quality, true);
            String filtering=getSharedPreferences("graphics",MODE_PRIVATE).getString("filtering","sharp");
            android.system.Os.setenv("DETHRACE_IMAGE_FILTER", "smooth".equals(filtering)?"smooth":"sharp", true);
        } catch (Exception e) {
            Log.e(TAG, "Failed to setenv", e);
        }

        try { diagnostics = Diagnostics.start(this); } catch (Exception e) {
            android.widget.Toast.makeText(this,"Profiling could not start: "+e.getMessage(),android.widget.Toast.LENGTH_LONG).show();
        }

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        super.onCreate(savedInstanceState);
        gameRunning=true;

        // Draw under cutout (notch / camera hole)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        applyImmersiveMode();

        // Overlay the touch-control hints on top of the SDL surface
        if (mLayout != null) {
            mOverlay = new TouchOverlayView(this, new TouchOverlayView.Listener() {
                public void input(int mask, int axis, boolean active) { nativeTouch(mask,axis,active); }
                public void reset() { nativeResetTouch(); }
            });
            mOverlay.setVisibility(View.GONE);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
            mLayout.addView(mOverlay, lp);
            monitorMode = getSharedPreferences("graphics", MODE_PRIVATE).getString("monitor", "off");
            if (!"off".equals(monitorMode)) {
                mMonitor = new android.widget.TextView(this);
                mMonitor.setTextColor(android.graphics.Color.WHITE);
                mMonitor.setTextSize(13);
                mMonitor.setTypeface(android.graphics.Typeface.MONOSPACE);
                mMonitor.setBackgroundColor(0xB0000000);
                mMonitor.setPadding(10, 4, 10, 4);
                mMonitor.setClickable(false); mMonitor.setFocusable(false);
                android.widget.RelativeLayout.LayoutParams info = new android.widget.RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                info.addRule(android.widget.RelativeLayout.ALIGN_PARENT_TOP);
                info.addRule(android.widget.RelativeLayout.ALIGN_PARENT_RIGHT);
                int margin = (int)(8 * getResources().getDisplayMetrics().density);
                info.topMargin = margin; info.rightMargin = margin;
                mMonitor.setOnApplyWindowInsetsListener((view, insets) -> {
                    android.view.DisplayCutout cutout = Build.VERSION.SDK_INT >= 28 ? insets.getDisplayCutout() : null;
                    ViewGroup.MarginLayoutParams position = (ViewGroup.MarginLayoutParams)view.getLayoutParams();
                    position.topMargin = margin + (cutout == null ? 0 : cutout.getSafeInsetTop());
                    position.rightMargin = margin + (cutout == null ? 0 : cutout.getSafeInsetRight());
                    view.setLayoutParams(position);
                    return insets;
                });
                mLayout.addView(mMonitor, info);
                mMonitor.requestApplyInsets();
            }
            mUiHandler.postDelayed(mPollRaceState, 500);
        }
    }

    private final Runnable mPollMonitor = new Runnable() {
        @Override public void run() {
            if (mMonitor == null || isFinishing() || isDestroyed()) return;
            String fps = "", temperature = "";
            if ("fps".equals(monitorMode) || "both".equals(monitorMode)) {
                int tenths = 0;
                try { tenths = nativeFpsTenths(); } catch (UnsatisfiedLinkError ignored) { }
                fps = String.format(java.util.Locale.US, "FPS %.1f", tenths / 10f);
            }
            if ("temperature".equals(monitorMode) || "both".equals(monitorMode)) {
                temperature = "Battery N/A";
                android.content.Intent battery = registerReceiver(null,
                    new android.content.IntentFilter(android.content.Intent.ACTION_BATTERY_CHANGED));
                if (battery != null && battery.getBooleanExtra(android.os.BatteryManager.EXTRA_PRESENT, false)
                        && battery.hasExtra(android.os.BatteryManager.EXTRA_TEMPERATURE)) {
                    int tenths = battery.getIntExtra(android.os.BatteryManager.EXTRA_TEMPERATURE, Integer.MIN_VALUE);
                    if (tenths != Integer.MIN_VALUE)
                        temperature = String.format(java.util.Locale.US, "Battery %.1f °C", tenths / 10f);
                }
            }
            mMonitor.setText(fps + (!fps.isEmpty() && !temperature.isEmpty() ? "  |  " : "") + temperature);
            mUiHandler.postDelayed(this, 1000);
        }
    };

    private final Runnable mPollRaceState = new Runnable() {
        @Override
        public void run() {
            try {
                boolean racing = nativeIsInRace();
                boolean impact = nativeTakeImpact();
                if (racing && impact && mOverlay != null) mOverlay.impactFeedback();
                if (mOverlay != null) {
                    int target = racing && !nativeUsingGamepad() ? View.VISIBLE : View.GONE;
                    if (mOverlay.getVisibility() != target) {
                        mOverlay.reset();
                        mOverlay.setVisibility(target);
                    }
                }
            } catch (UnsatisfiedLinkError ignore) {
                // libmain.so not yet ready or symbol not exported
            } catch (Throwable ignore) {
            }
            mUiHandler.postDelayed(this, 100);
        }
    };

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus && mOverlay != null) mOverlay.reset();
        if (hasFocus) {
            applyImmersiveMode();
        }
    }

    private void applyImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }
}
