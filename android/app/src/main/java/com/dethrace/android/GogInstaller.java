package com.dethrace.android;

import java.io.*;
import java.util.concurrent.TimeUnit;

/** Reads installer data with bundled innoextract; never executes the Windows EXE. */
final class GogInstaller {
    static void extract(InputStream input,File unpacked,File executable)throws IOException {
        if(executable==null||!executable.canExecute())throw new IOException("GOG installer extraction is unavailable in this build. Import a ZIP of the installed game instead.");
        File installer=File.createTempFile("carma-gog-",".exe",unpacked.getAbsoluteFile().getParentFile());
        Process process=null;
        try {
            try(OutputStream out=new BufferedOutputStream(new FileOutputStream(installer))){GameDataZip.copy(input,out,GameDataZip.MAX_BYTES);}
            if(!unpacked.mkdirs())throw new IOException("Cannot create installer staging folder");
            ProcessBuilder builder=new ProcessBuilder(executable.getAbsolutePath(),"--extract","--silent","--no-extract-unknown","--exclude-temp",
                "--include","DATA","--include","MUSIC","--output-dir",unpacked.getAbsolutePath(),installer.getAbsolutePath());
            builder.redirectErrorStream(true);process=builder.start();
            final Process running=process;final StringBuilder messages=new StringBuilder();
            Thread reader=new Thread(()->{
                try(Reader r=new InputStreamReader(running.getInputStream(),"UTF-8")){
                    char[] buffer=new char[1024];int n;while((n=r.read(buffer))!=-1)synchronized(messages){if(messages.length()<4096)messages.append(buffer,0,Math.min(n,4096-messages.length()));}
                }catch(IOException ignored){}
            },"gog-extractor-output");reader.setDaemon(true);reader.start();
            long deadline=System.nanoTime()+TimeUnit.MINUTES.toNanos(5);
            while(isRunning(process)){
                Thread.sleep(250);
                if(System.nanoTime()>deadline||size(unpacked)>GameDataZip.MAX_BYTES){process.destroy();throw new IOException("Installer extraction exceeded time or 2 GiB space limit");}
            }
            reader.join(2000);
            if(process.exitValue()!=0){String detail;synchronized(messages){detail=messages.toString().trim();}
                throw new IOException("Cannot unpack this GOG installer. Use the complete offline installer; if it needs companion BIN files, import a ZIP of the installed game instead. "+detail);}
            if(size(unpacked)>GameDataZip.MAX_BYTES)throw new IOException("Installer data exceeds 2 GiB limit");
        }catch(InterruptedException e){Thread.currentThread().interrupt();throw new IOException("Installer import interrupted",e);}
        finally {
            if(process!=null&&isRunning(process)){
                process.destroy();
                try{process.waitFor();}catch(InterruptedException e){Thread.currentThread().interrupt();}
            }
            if(!installer.delete()&&installer.exists())installer.deleteOnExit();
        }
    }
    private static boolean isRunning(Process p){try{p.exitValue();return false;}catch(IllegalThreadStateException e){return true;}}
    private static long size(File root)throws IOException {return size(root,0,new int[]{0});}
    private static long size(File root,int depth,int[] count)throws IOException {
        if(depth>64)throw new IOException("Installer folder nesting exceeds limit");
        long total=0;File[] files=root.listFiles();if(files==null)throw new IOException("Cannot inspect installer staging");
        for(File f:files){if(++count[0]>50000||!f.getCanonicalPath().equals(new File(root.getCanonicalFile(),f.getName()).getPath()))throw new IOException("Unsafe or excessive installer files");total+=f.isDirectory()?size(f,depth+1,count):f.length();if(total>GameDataZip.MAX_BYTES)return total;}
        return total;
    }
    private GogInstaller(){}
}
