#!/usr/bin/env python3
"""Run the actual View's pointer logic against minimal Android host stubs."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
files={
'android/content/Context.java': '''package android.content; public class Context { public static final int MODE_PRIVATE=0; public Pref getSharedPreferences(String s,int i){return new Pref();} public static class Pref {public boolean getBoolean(String s,boolean b){return b;}} }''',
'android/graphics/RectF.java': '''package android.graphics; public class RectF {public float left,top,right,bottom; public void set(float l,float t,float r,float b){left=l;top=t;right=r;bottom=b;} public float width(){return right-left;} public float height(){return bottom-top;} public float centerX(){return (left+right)/2;} public float centerY(){return (top+bottom)/2;} public boolean contains(float x,float y){return x>=left&&x<right&&y>=top&&y<bottom;}}''',
'android/graphics/Paint.java': '''package android.graphics; public class Paint {public static final int ANTI_ALIAS_FLAG=1; public enum Style{FILL,STROKE} public enum Align{CENTER} float size; public Paint(int i){} public void setStyle(Style s){} public void setColor(int i){} public void setStrokeWidth(float f){} public void setTextAlign(Align a){} public void setFakeBoldText(boolean b){} public void setTextSize(float f){size=f;} public float getTextSize(){return size;} public float ascent(){return -size;} public float descent(){return 0;} }''',
'android/graphics/Color.java': '''package android.graphics; public class Color {public static final int WHITE=1; public static int argb(int a,int r,int g,int b){return 0;} public static int rgb(int r,int g,int b){return 0;} }''',
'android/graphics/Canvas.java': '''package android.graphics; public class Canvas {public void drawRoundRect(RectF r,float x,float y,Paint p){} public void drawText(String t,float x,float y,Paint p){} public void drawLine(float a,float b,float c,float d,Paint p){} public void drawCircle(float x,float y,float r,Paint p){} }''',
'android/os/SystemClock.java': '''package android.os; public class SystemClock {public static long now=1000; public static long uptimeMillis(){return now;}}''',
'android/util/SparseIntArray.java': '''package android.util; import java.util.*; public class SparseIntArray {TreeMap<Integer,Integer> m=new TreeMap<>(); public int size(){return m.size();} public int valueAt(int i){return new ArrayList<Integer>(m.values()).get(i);} public int get(int k,int d){return m.getOrDefault(k,d);} public void put(int k,int v){m.put(k,v);} public void delete(int k){m.remove(k);} public void clear(){m.clear();}}''',
'android/view/HapticFeedbackConstants.java': '''package android.view; public class HapticFeedbackConstants {public static final int LONG_PRESS=1,VIRTUAL_KEY=2;}''',
'android/view/View.java': '''package android.view; import android.content.Context; import android.graphics.Canvas; public class View {int w,h; public Runnable pending; public View(Context c){} public void setFocusable(boolean b){} protected void onSizeChanged(int w,int h,int ow,int oh){} public void resize(int a,int b){w=a;h=b;onSizeChanged(a,b,0,0);} public int getHeight(){return h;} public void invalidate(){} public boolean performHapticFeedback(int x){return true;} public void postDelayed(Runnable r,long t){pending=r;} public void removeCallbacks(Runnable r){if(pending==r)pending=null;} public boolean onTouchEvent(MotionEvent e){return false;} protected void onDetachedFromWindow(){} protected void onDraw(Canvas c){} }''',
'android/view/MotionEvent.java': '''package android.view; public class MotionEvent {public static final int ACTION_DOWN=0,ACTION_UP=1,ACTION_MOVE=2,ACTION_CANCEL=3,ACTION_POINTER_DOWN=5,ACTION_POINTER_UP=6; public int action,index; public int[] ids; public float[] xs,ys; public MotionEvent(int a,int i,int[] id,float[] x,float[] y){action=a;index=i;ids=id;xs=x;ys=y;} public int getActionIndex(){return index;} public int getPointerId(int i){return ids[i];} public int getActionMasked(){return action;} public int getPointerCount(){return ids.length;} public float getX(int i){return xs[i];} public float getY(int i){return ys[i];}}''',
'OverlayTest.java': '''import android.content.*; import android.view.*; import android.graphics.*; import android.os.*; import java.lang.reflect.*; import com.dethrace.android.TouchOverlayView;
public class OverlayTest {
 static int mask,axis; static boolean active;
 static TouchOverlayView v=new TouchOverlayView(new Context(),new TouchOverlayView.Listener(){public void input(int m,int a,boolean b){mask=m;axis=a;active=b;}public void reset(){mask=axis=0;active=false;}});
 static void check(boolean b){if(!b)throw new AssertionError();}
 static RectF[] rects()throws Exception {Field f=v.getClass().getDeclaredField("buttons");f.setAccessible(true);return (RectF[])f.get(v);}
 static void event(int a,int idx,int[] ids,float[] x,float[] y){v.onTouchEvent(new MotionEvent(a,idx,ids,x,y));}
 public static void main(String[] args)throws Exception {
  for(int w:new int[]{2340,1920,1440}) {v.resize(w,1080);RectF[] r=rects(); for(int i=0;i<8;i++){check(r[i].left>=0&&r[i].right<=w&&r[i].top>=0&&r[i].bottom<=1080);for(int j=i+1;j<8;j++)check(r[i].right<=r[j].left||r[i].left>=r[j].right||r[i].bottom<=r[j].top||r[i].top>=r[j].bottom);}}
  v.resize(2340,1080);RectF[] r=rects();float x=r[0].centerX(),y=r[0].centerY();
  event(0,0,new int[]{7},new float[]{x},new float[]{y});check(mask==1);
  event(5,1,new int[]{7,19},new float[]{x,55},new float[]{y,980});check(mask==1&&active&&axis<0);
  event(6,1,new int[]{7,19},new float[]{x,55},new float[]{y,980});check(mask==1&&!active);
  // Two pointers on gas: releasing one cannot cancel the other.
  event(5,1,new int[]{7,21},new float[]{x,x},new float[]{y,y});
  event(6,0,new int[]{7,21},new float[]{x,x},new float[]{y,y});check(mask==1);
  event(1,0,new int[]{21},new float[]{x},new float[]{y});check(mask==0);
  x=r[4].centerX();y=r[4].centerY();
  event(0,0,new int[]{1},new float[]{x},new float[]{y});check(mask==0);
  SystemClock.now+=610;v.pending.run();check(mask==16);
  event(3,0,new int[]{1},new float[]{x},new float[]{y});check(mask==0&&!active&&v.pending==null);
  // Digital mode uses full left/right, neutral center and releases cleanly.
  new Context().getSharedPreferences("graphics",0).edit().putString("steering","digital").apply();
  v=new TouchOverlayView(new Context(),new TouchOverlayView.Listener(){public void input(int m,int a,boolean b){mask=m;axis=a;active=b;}public void reset(){mask=axis=0;active=false;}});
  v.restoreDefaults();v.resize(2340,1080);r=rects();
  Field sf=v.getClass().getDeclaredField("steer");sf.setAccessible(true);RectF sr=(RectF)sf.get(v);
  event(0,0,new int[]{9},new float[]{sr.centerX()-sr.width()*.15f},new float[]{sr.centerY()});check(active&&axis==-65535);
  event(2,0,new int[]{9},new float[]{sr.centerX()},new float[]{sr.centerY()});check(active&&axis==0);
  event(2,0,new int[]{9},new float[]{sr.centerX()+sr.width()*.15f},new float[]{sr.centerY()});check(active&&axis==65535);
  event(1,0,new int[]{9},new float[]{sr.centerX()},new float[]{sr.centerY()});check(!active&&axis==0);
  System.out.println("PASS: overlay geometry at 70/100/140%, mirror presets, drag/save, normalized resize, overlap rejection, multi-touch and recovery");
 }
}'''
}
# Stubs include persisted preferences and vector drawing APIs used by the editor.
files['android/content/SharedPreferences.java'] = """package android.content; import java.util.*; public class SharedPreferences {
 public Map<String,Object> values=new HashMap<>();
 public String getString(String k,String d){return (String)values.getOrDefault(k,d);}
 public boolean getBoolean(String k,boolean d){return (Boolean)values.getOrDefault(k,d);}
 public int getInt(String k,int d){return (Integer)values.getOrDefault(k,d);}
 public float getFloat(String k,float d){return (Float)values.getOrDefault(k,d);}
 public Editor edit(){return new Editor();} public class Editor {
 Map<String,Object> pending=new HashMap<>();
 public Editor putString(String k,String v){pending.put(k,v);return this;}
 public Editor putInt(String k,int v){pending.put(k,v);return this;}
 public Editor putFloat(String k,float v){pending.put(k,v);return this;}
 public Editor putBoolean(String k,boolean v){pending.put(k,v);return this;}
 public void apply(){values.putAll(pending);}} }"""
files['android/content/Context.java'] = """package android.content; public class Context {
 public static final int MODE_PRIVATE=0; static SharedPreferences p=new SharedPreferences();
 public SharedPreferences getSharedPreferences(String s,int i){return p;} }"""
files['android/graphics/RectF.java']=files['android/graphics/RectF.java'].replace('public float left','public RectF(){} public RectF(float l,float t,float r,float b){set(l,t,r,b);} public float left')
files['android/graphics/Paint.java']=files['android/graphics/Paint.java'].replace('public enum Style','public enum Cap{ROUND,BUTT} public enum Join{ROUND} public void setStrokeCap(Cap c){} public void setStrokeJoin(Join j){} public enum Style')
files['android/graphics/Path.java']='package android.graphics; public class Path {public void moveTo(float x,float y){} public void lineTo(float x,float y){} public void close(){}}'
files['android/graphics/Canvas.java']=files['android/graphics/Canvas.java'].replace('public class Canvas {','public class Canvas {public void save(){} public void restore(){} public void translate(float x,float y){} public void scale(float x,float y){} public void drawPath(Path p,Paint q){} public void drawArc(RectF r,float s,float a,boolean c,Paint p){} ')
files['android/view/View.java']=files['android/view/View.java'].replace('public int getHeight()','public int getWidth(){return w;} public int getHeight()')
files['OverlayTest.java']=files['OverlayTest.java'].replace('  v.resize(2340,1080);RectF[] r=rects();','''  // All standard and mirrored sizes must fit and remain non-overlapping.
  for(int w:new int[]{2340,1920,1440}) for(int percent:new int[]{70,100,140}) for(boolean mirror:new boolean[]{false,true}) {
    v.resize(w,1080);v.setSizePercent(percent);v.setPreset(mirror);check(v.saveLayout());
    for(RectF rr:rects())check(rr.left>=0 && rr.right<=w && rr.top>=0 && rr.bottom<=1080);
  }
  v.restoreDefaults();v.resize(2340,1080);RectF[] r=rects();''')
files['OverlayTest.java']=files['OverlayTest.java'].replace('  System.out.println(','''  // Drag a toolbar icon into a free center area; save and re-layout at half size.
  v.setEditing(true);x=r[5].centerX();y=r[5].centerY();
  event(0,0,new int[]{1},new float[]{x},new float[]{y});
  event(2,0,new int[]{1},new float[]{1100},new float[]{400});
  event(1,0,new int[]{1},new float[]{1100},new float[]{400});
  check(v.saveLayout());v.resize(1170,540);check(Math.abs(rects()[5].centerX()-550)<1);
  // A drag onto another button is rejected and restores the previous position.
  r=rects();x=r[5].centerX();y=r[5].centerY();float old=x;
  event(0,0,new int[]{1},new float[]{x},new float[]{y});
  event(2,0,new int[]{1},new float[]{r[0].centerX()},new float[]{r[0].centerY()});
  event(1,0,new int[]{1},new float[]{r[0].centerX()},new float[]{r[0].centerY()});
  check(Math.abs(r[5].centerX()-old)<1);check(v.saveLayout());check(mask==0);
  System.out.println(''')
with tempfile.TemporaryDirectory() as d:
    d=Path(d)
    for name,code in files.items():
        p=d/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(code)
    p=d/'com/dethrace/android/TouchOverlayView.java';p.parent.mkdir(parents=True);p.write_text((root/'android/app/src/main/java/com/dethrace/android/TouchOverlayView.java').read_text())
    jdk=os.environ.get('JAVA_HOME','/workspace/scratch/7688438f9497/toolchains/jdk-17')
    subprocess.run([jdk+'/bin/javac','-d',str(d)]+[str(p) for p in d.rglob('*.java')],check=True)
    subprocess.run([jdk+'/bin/java','-cp',str(d),'OverlayTest'],check=True)
