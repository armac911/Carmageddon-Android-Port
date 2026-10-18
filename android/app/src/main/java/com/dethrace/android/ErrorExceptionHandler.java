package com.dethrace.android;

import java.io.*;
import java.util.function.Supplier;

/** Capture is best effort; always retain the platform's uncaught-exception path. */
final class ErrorExceptionHandler implements Thread.UncaughtExceptionHandler {
    private final Supplier<File> session;
    private final Thread.UncaughtExceptionHandler previous;
    private final Runnable terminate;
    ErrorExceptionHandler(Supplier<File> session,Thread.UncaughtExceptionHandler previous,Runnable terminate){
        this.session=session;this.previous=previous;this.terminate=terminate;
    }
    @Override public void uncaughtException(Thread thread,Throwable error){
        try {
            File dir=session.get();
            if(dir!=null)try(FileOutputStream out=new FileOutputStream(new File(dir,"java-exception.txt"))){
                OutputStream bounded=new OutputStream(){long left=256*1024;
                    public void write(int b)throws IOException{if(left>0){out.write(b);left--;}}
                    public void write(byte[] b,int off,int length)throws IOException{int n=(int)Math.min(left,length);if(n>0){out.write(b,off,n);left-=n;}}
                };
                PrintStream text=new PrintStream(bounded,false,"UTF-8");
                text.println("Uncaught exception on thread: "+thread.getName());error.printStackTrace(text);text.flush();out.getFD().sync();
            }
        }catch(Throwable ignored){}
        finally {
            if(previous!=null)previous.uncaughtException(thread,error);
            else terminate.run();
        }
    }
}
