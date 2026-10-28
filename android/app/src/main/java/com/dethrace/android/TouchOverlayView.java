package com.dethrace.android;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Path;
import android.os.SystemClock;
import android.util.SparseIntArray;
import android.view.HapticFeedbackConstants;
import android.view.MotionEvent;
import android.view.View;

/** One geometry drives drawing and hit testing. Each pointer owns its action. */
public class TouchOverlayView extends View {
    public interface Listener {
        void input(int mask, int steering, boolean active);
        void reset();
    }
    private final Listener listener;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF[] buttons = new RectF[8];
    private final RectF steer = new RectF();
    private final SparseIntArray pointers = new SparseIntArray();
    private final String[] labels = {"GAS", "BRAKE\nREVERSE", "HAND\nBRAKE", "HOLD\nREPAIR", "HOLD\nRECOVER", "CAMERA", "MAP", "PAUSE"};
    private int steeringPointer = -1, mask;
    private float axis;
    private long recoverStart;
    private boolean recoverReady;
    private final boolean haptics;
    private final boolean digitalSteering;
    private final android.content.SharedPreferences preferences;
    private float controlScale = 1f;
    private boolean mirrored, editing;
    private final float[] customX = new float[9], customY = new float[9];
    private int dragged = -1, dragPointer = -1;
    private float dragX, dragY, beforeX, beforeY;
    private String editMessage = "Drag any control";

    public void setEditing(boolean value) { editing = value; reset(); }
    public String getEditMessage() { return editMessage; }
    public int getSizePercent() { return Math.round(controlScale * 100); }
    public void setSizePercent(int value) {
        controlScale = Math.max(.7f, Math.min(1.4f,value/100f));
        layoutControls(getWidth(),getHeight()); invalidate();
    }
    public void setPreset(boolean leftHanded) {
        mirrored = leftHanded;
        java.util.Arrays.fill(customX, -1f); java.util.Arrays.fill(customY, -1f);
        layoutControls(getWidth(),getHeight()); invalidate();
    }
    public void restoreDefaults() { controlScale = 1f; setPreset(false); }
    public boolean saveLayout() {
        if (overlaps()) { editMessage = "Separate the overlapping controls before saving"; invalidate(); return false; }
        android.content.SharedPreferences.Editor editor = preferences.edit();
        editor.putInt("size_percent",getSizePercent()).putBoolean("mirrored",mirrored);
        for (int i=0;i<9;i++) {
            editor.putFloat("control_x_"+i,customX[i]);
            editor.putFloat("control_y_"+i,customY[i]);
        }
        editor.apply(); return true;
    }
    private RectF rect(int i) { return i==8 ? steer : buttons[i]; }
    private boolean overlaps() {
        for (int i=0;i<9;i++) for (int j=i+1;j<9;j++) {
            RectF a=rect(i), b=rect(j);
            if (a.left<b.right && a.right>b.left && a.top<b.bottom && a.bottom>b.top) return true;
        }
        return false;
    }
    private void center(RectF r,float x,float y,int w,int h) {
        float rw=r.width(), rh=r.height();
        x=Math.max(rw/2,Math.min(w-rw/2,x)); y=Math.max(rh/2,Math.min(h-rh/2,y));
        r.set(x-rw/2,y-rh/2,x+rw/2,y+rh/2);
    }

    public TouchOverlayView(Context context, Listener listener) {
        super(context);
        this.listener = listener;
        digitalSteering = "digital".equals(context.getSharedPreferences("graphics", Context.MODE_PRIVATE).getString("steering", "analog"));
        preferences = context.getSharedPreferences("controls", Context.MODE_PRIVATE);
        controlScale = Math.max(.7f,Math.min(1.4f,preferences.getInt("size_percent",100)/100f));
        mirrored = preferences.getBoolean("mirrored",false);
        for (int i=0;i<9;i++) {
            customX[i]=preferences.getFloat("control_x_"+i,-1);
            customY[i]=preferences.getFloat("control_y_"+i,-1);
        }
        haptics = context.getSharedPreferences("controls", Context.MODE_PRIVATE).getBoolean("haptics", true);
        setFocusable(false);
        for (int i = 0; i < buttons.length; i++) buttons[i] = new RectF();
    }
    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        reset();
        layoutControls(w,h);
    }
    private void layoutControls(int w,int h) {
        if (w<=0 || h<=0) return;
        float m = h * .04f, b = h * .19f * controlScale, gap = h * .025f * controlScale, bottom = h - m;
        steer.set(m, bottom - b * 1.1f, m + b * 2.1f, bottom);
        buttons[0].set(w - m - b, bottom - b * 1.35f, w - m, bottom);
        buttons[1].set(w - m - 2*b - gap, bottom - b * 1.1f, w - m - b - gap, bottom);
        buttons[2].set(buttons[1].left, buttons[1].top - gap - b * .65f, buttons[1].right, buttons[1].top - gap);
        float small = b * .85f;
        buttons[3].set(m, steer.top - gap - small * .7f, m + b, steer.top - gap);
        buttons[4].set(m + b + gap, buttons[3].top, steer.right, buttons[3].bottom);
        for (int i = 5; i < 8; i++) {
            float right = w - m - (7 - i) * (small + gap);
            buttons[i].set(right - small, m, right, m + small * .7f);
        }
        for (int i=0;i<9;i++) {
            RectF r=rect(i);
            if (mirrored) r.set(w-r.right,r.top,w-r.left,r.bottom);
            if (customX[i]>=0 && customY[i]>=0) center(r,customX[i]*w,customY[i]*h,w,h);
        }
    }
    private int hit(float x, float y) {
        for (int i = 0; i < buttons.length; i++) if (buttons[i].contains(x,y)) return i;
        return -1;
    }
    private boolean held(int action) {
        for (int i = 0; i < pointers.size(); i++) if (pointers.valueAt(i) == action) return true;
        return false;
    }
    private void feedback(boolean strong) {
        if (haptics) performHapticFeedback(strong ? HapticFeedbackConstants.LONG_PRESS : HapticFeedbackConstants.VIRTUAL_KEY);
    }
    public void impactFeedback() { feedback(true); }
    private final Runnable recoverCheck = () -> {
        if (held(4) && SystemClock.uptimeMillis() - recoverStart >= 600) {
            recoverReady = true;
            feedback(true);
            send();
        }
    };
    private void setPointer(int id, int action) {
        int previous = pointers.get(id, -1);
        if (previous == action) return;
        boolean already = held(action);
        pointers.put(id, action);
        if (action >= 0 && !already) feedback(false);
        if (!held(4)) { recoverReady = false; removeCallbacks(recoverCheck); }
        if (action == 4 && !already) {
            recoverStart = SystemClock.uptimeMillis();
            recoverReady = false;
            postDelayed(recoverCheck, 610);
        }
    }
    private void updateAxis(float x) {
        float raw = Math.max(-1, Math.min(1, (x - steer.centerX()) / (steer.width() * .40f)));
        axis = raw; // Native tuning applies the selected deadzone and response.
    }
    private void send() {
        mask = 0;
        for (int i = 0; i < pointers.size(); i++) {
            int action = pointers.valueAt(i);
            if (action >= 0 && (action != 4 || recoverReady)) mask |= 1 << action;
        }
        listener.input(mask, Math.round(axis * 65535), steeringPointer >= 0);
        invalidate();
    }
    public void reset() {
        pointers.clear(); steeringPointer = -1; axis = 0; mask = 0;
        recoverReady = false; removeCallbacks(recoverCheck);
        listener.reset(); invalidate();
    }
    private boolean editTouch(MotionEvent event) {
        int index=event.getActionIndex();
        if (event.getActionMasked()==MotionEvent.ACTION_DOWN) {
            dragged=hit(event.getX(index),event.getY(index));
            if (dragged<0 && steer.contains(event.getX(index),event.getY(index))) dragged=8;
            if (dragged>=0) {
                RectF r=rect(dragged);
                dragPointer=event.getPointerId(index);
                dragX=event.getX(index)-r.centerX(); dragY=event.getY(index)-r.centerY();
                beforeX=customX[dragged]; beforeY=customY[dragged];
                editMessage=dragged==8 ? "Steering" : labels[dragged].replace('\n',' ');
            }
        } else if (event.getActionMasked()==MotionEvent.ACTION_MOVE && dragged>=0) {
            for (int i=0;i<event.getPointerCount();i++) if (event.getPointerId(i)==dragPointer) {
                RectF r=rect(dragged);
                center(r,event.getX(i)-dragX,event.getY(i)-dragY,getWidth(),getHeight());
                customX[dragged]=r.centerX()/getWidth(); customY[dragged]=r.centerY()/getHeight();
            }
        } else if (event.getActionMasked()==MotionEvent.ACTION_UP || event.getActionMasked()==MotionEvent.ACTION_CANCEL) {
            if (dragged>=0 && (event.getActionMasked()==MotionEvent.ACTION_CANCEL || overlaps())) {
                customX[dragged]=beforeX; customY[dragged]=beforeY;
                layoutControls(getWidth(),getHeight());
                editMessage="Keep controls apart";
            }
            dragged=dragPointer=-1;
        }
        invalidate(); return true;
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if (editing) return editTouch(event);
        int index = event.getActionIndex(), id = event.getPointerId(index);
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                if (steeringPointer < 0 && steer.contains(event.getX(index), event.getY(index))) {
                    steeringPointer = id; updateAxis(event.getX(index)); feedback(false);
                } else setPointer(id, hit(event.getX(index), event.getY(index)));
                break;
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int pointer = event.getPointerId(i);
                    if (pointer == steeringPointer) updateAxis(event.getX(i));
                    else setPointer(pointer, hit(event.getX(i), event.getY(i)));
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                if (id == steeringPointer) { steeringPointer = -1; axis = 0; }
                pointers.delete(id);
                if (!held(4)) { recoverReady = false; removeCallbacks(recoverCheck); }
                break;
            case MotionEvent.ACTION_CANCEL: reset(); return true;
        }
        send();
        return true;
    }
    @Override protected void onDetachedFromWindow() { reset(); super.onDetachedFromWindow(); }
    private void panel(Canvas canvas, RectF r, boolean active) {
        float corner = getHeight() * .025f;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(active ? Color.argb(210,215,92,28) : Color.argb(155,15,19,24));
        canvas.drawRoundRect(r,corner,corner,paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(getHeight()*.003f);
        paint.setColor(active ? Color.rgb(255,190,105) : Color.argb(180,225,230,235));
        canvas.drawRoundRect(r,corner,corner,paint);
    }
    private void line(Canvas c,float x,float y,float x2,float y2) { c.drawLine(x,y,x2,y2,paint); }
    private void icon(Canvas c,RectF r,int action) {
        c.save(); c.translate(r.centerX(),r.centerY());
        float unit=Math.min(r.width(),r.height())*.34f;
        c.scale(unit,unit);
        paint.setColor(Color.WHITE); paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(.12f); paint.setStrokeCap(Paint.Cap.ROUND); paint.setStrokeJoin(Paint.Join.ROUND);
        Path path=new Path();
        switch(action) {
            case 0: // Accelerator pedal, tread marks and forward arrow.
                c.drawRoundRect(new RectF(-.38f,-.9f,.38f,.9f),.12f,.12f,paint);
                for (float y=-.45f;y<.7f;y+=.3f) line(c,-.22f,y,.22f,y);
                line(c,.66f,.3f,.66f,-.5f); line(c,.47f,-.3f,.66f,-.5f); line(c,.85f,-.3f,.66f,-.5f); break;
            case 1: // Broad brake pedal and reverse arrow.
                c.drawRoundRect(new RectF(-.8f,-.65f,.8f,.15f),.12f,.12f,paint);
                for(float x=-.5f;x<.7f;x+=.32f) line(c,x,-.46f,x,-.06f);
                line(c,.65f,.65f,-.65f,.65f); line(c,-.35f,.4f,-.65f,.65f); line(c,-.35f,.9f,-.65f,.65f); break;
            case 2: // Parking brake symbol.
                c.drawCircle(0,0,.65f,paint);
                c.drawArc(new RectF(-1,-1,1,1),145,70,false,paint);
                c.drawArc(new RectF(-1,-1,1,1),-35,70,false,paint);
                paint.setStyle(Paint.Style.FILL); paint.setTextAlign(Paint.Align.CENTER); paint.setTextSize(1f); paint.setFakeBoldText(true);
                c.drawText("P",0,.35f,paint); break;
            case 3: // Open-ended wrench.
                path.moveTo(-.65f,.8f); path.lineTo(.25f,-.1f); path.lineTo(.65f,-.08f);
                path.lineTo(.9f,-.4f); path.lineTo(.9f,-.8f); path.lineTo(.55f,-.5f);
                path.lineTo(.3f,-.75f); path.lineTo(.6f,-1f); path.lineTo(.17f,-.93f);
                path.lineTo(-.1f,-.6f); path.lineTo(-.05f,-.25f); path.lineTo(-.9f,.6f); path.close();c.drawPath(path,paint); break;
            case 4: // Recovery/reset arrow.
                c.drawArc(new RectF(-.8f,-.8f,.8f,.8f),-40,290,false,paint);
                path.moveTo(.64f,-.54f);path.lineTo(.72f,-1f);path.moveTo(.64f,-.54f);path.lineTo(.16f,-.6f);c.drawPath(path,paint);
                line(c,-.25f,.25f,.25f,-.25f);line(c,-.25f,-.25f,.25f,.25f);break;
            case 5:
                c.drawRoundRect(new RectF(-.95f,-.55f,.95f,.65f),.15f,.15f,paint);
                c.drawCircle(0,.05f,.35f,paint);line(c,-.5f,-.55f,-.35f,-.85f);line(c,-.35f,-.85f,.35f,-.85f);line(c,.35f,-.85f,.5f,-.55f);break;
            case 6:
                path.moveTo(-.9f,-.65f);path.lineTo(-.3f,-.9f);path.lineTo(.3f,-.65f);path.lineTo(.9f,-.9f);
                path.lineTo(.9f,.65f);path.lineTo(.3f,.9f);path.lineTo(-.3f,.65f);path.lineTo(-.9f,.9f);path.close();c.drawPath(path,paint);
                line(c,-.3f,-.9f,-.3f,.65f);line(c,.3f,-.65f,.3f,.9f);break;
            case 7:
                paint.setStrokeWidth(.3f);line(c,-.35f,-.7f,-.35f,.7f);line(c,.35f,-.7f,.35f,.7f);break;
        }
        c.restore();
        paint.setStrokeCap(Paint.Cap.BUTT);
    }
    @Override protected void onDraw(Canvas canvas) {
        for (int i = 0; i < buttons.length; i++) {
            panel(canvas,buttons[i],held(i) || (editing && dragged==i));
            icon(canvas,buttons[i],i);
        }
        if (digitalSteering) {
            float gap=steer.width()*.025f;
            RectF left=new RectF(steer.left,steer.top,steer.centerX()-gap,steer.bottom);
            RectF right=new RectF(steer.centerX()+gap,steer.top,steer.right,steer.bottom);
            panel(canvas,left,axis<0 || (editing && dragged==8));
            panel(canvas,right,axis>0 || (editing && dragged==8));
            paint.setColor(Color.WHITE); paint.setStrokeWidth(getHeight()*.006f);
            float d=steer.height()*.22f, y=steer.centerY();
            for (int direction : new int[]{-1,1}) {
                float x=direction<0 ? left.centerX() : right.centerX();
                line(canvas,x-direction*d,y-d,x+direction*d,y);
                line(canvas,x+direction*d,y,x-direction*d,y+d);
            }
            return;
        }
        panel(canvas,steer,steeringPointer>=0 || (editing && dragged==8));
        paint.setColor(Color.argb(150,230,235,240)); paint.setStrokeWidth(getHeight()*.005f);
        canvas.drawLine(steer.left+steer.width()*.12f,steer.centerY(),steer.right-steer.width()*.12f,steer.centerY(),paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(steeringPointer >= 0 ? Color.rgb(255,170,80) : Color.WHITE);
        canvas.drawCircle(steer.centerX()+axis*steer.width()*.38f,steer.centerY(),steer.height()*.23f,paint);
    }
}
