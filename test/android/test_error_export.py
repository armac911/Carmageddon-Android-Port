"""Production Java storage/export and exception delegation without Android runtime."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
fixture=r'''
package com.dethrace.android;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;
public final class ErrorCaptureTest {
 static void check(boolean condition){if(!condition)throw new AssertionError();}
 public static void main(String[] args)throws Exception {
  File root=new File(args[0]);root.mkdirs();File dir=new File(root,"session");dir.mkdir();
  File log=new File(dir,"logcat.log");ErrorLogStore.append(log,"12345678",10);ErrorLogStore.append(log,"abcdefgh",10);
  check(log.length()==8&&new File(log.getPath()+".previous").length()==8);
  ErrorLogStore.append(log,"xxxxxxxxxxxxxxxx",10);check(log.length()==10);
  Files.write(new File(dir,"metadata.json").toPath(),"{}".getBytes());
  Files.write(new File(dir,"private-save.dat").toPath(),"not-for-export".getBytes());
  Files.write(new File(dir,"exit-123-5.json").toPath(),"{}".getBytes());
  Files.write(new File(dir,"exit-123-5.trace.pb").toPath(),new byte[]{0,1,2});
  File outside=new File(root,"outside.txt");Files.write(outside.toPath(),"not-for-export".getBytes());
  Files.createSymbolicLink(new File(dir,"native-signal.txt").toPath(),outside.toPath());
  ByteArrayOutputStream bytes=new ByteArrayOutputStream();ErrorLogStore.export(bytes,new File[]{dir});
  Set<String> names=new HashSet<>();try(ZipInputStream zip=new ZipInputStream(new ByteArrayInputStream(bytes.toByteArray()))){ZipEntry e;while((e=zip.getNextEntry())!=null){names.add(e.getName());ByteArrayOutputStream data=new ByteArrayOutputStream();ErrorLogStore.copyBounded(zip,data,100000);check(!data.toString("UTF-8").contains("not-for-export"));}}
  check(names.contains("ABOUT.txt")&&names.contains("session/logcat.log")&&names.contains("session/exit-123-5.trace.pb"));
  check(!names.contains("session/private-save.dat")&&!names.contains("session/native-signal.txt"));
  File history=new File(root,"history");history.mkdir();for(int i=0;i<15;i++)new File(history,String.format("%03d",i)).mkdir();
  check(ErrorLogStore.sessions(history).length==12);ErrorLogStore.prune(history);check(history.listFiles().length==12&&!new File(history,"000").exists());
  int[] delegated={0},terminated={0};Thread.UncaughtExceptionHandler parent=(t,e)->delegated[0]++;
  new ErrorExceptionHandler(()->null,parent,()->terminated[0]++).uncaughtException(Thread.currentThread(),new RuntimeException("off"));check(delegated[0]==1&&terminated[0]==0);
  new ErrorExceptionHandler(()->dir,parent,()->terminated[0]++).uncaughtException(Thread.currentThread(),new RuntimeException("test-error"));
  File stack=new File(dir,"java-exception.txt");check(new String(Files.readAllBytes(stack.toPath())).contains("test-error"));check(delegated[0]==2);
  new ErrorExceptionHandler(()->new File(root,"missing"),parent,()->terminated[0]++).uncaughtException(Thread.currentThread(),new RuntimeException("write-fails"));check(delegated[0]==3);
  new ErrorExceptionHandler(()->{throw new OutOfMemoryError();},parent,()->terminated[0]++).uncaughtException(Thread.currentThread(),new RuntimeException());check(delegated[0]==4);
  new ErrorExceptionHandler(()->dir,null,()->terminated[0]++).uncaughtException(Thread.currentThread(),new RuntimeException("x".repeat(300000)));check(terminated[0]==1&&stack.length()<=256*1024);
  System.out.println("PASS Java errors: opt-out delegation, failure delegation, bounded stack/log rotation, retention, allowlisted ZIP export, trace bytes and path containment");
 }
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'ErrorCaptureTest.java').write_text(fixture)
 java=Path(os.environ['JAVA_HOME'])/'bin' if os.environ.get('JAVA_HOME') else None
 sources=root/'android/app/src/main/java/com/dethrace/android'
 subprocess.run([str(java/'javac') if java else 'javac','-d',str(p),str(sources/'ErrorLogStore.java'),str(sources/'ErrorExceptionHandler.java'),str(p/'ErrorCaptureTest.java')],check=True)
 subprocess.run([str(java/'java') if java else 'java','-cp',str(p),'com.dethrace.android.ErrorCaptureTest',str(p/'files')],check=True)
