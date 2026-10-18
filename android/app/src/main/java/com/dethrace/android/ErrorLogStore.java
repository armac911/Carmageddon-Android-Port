package com.dethrace.android;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.zip.*;

/** Bounded files and allowlisted ZIP export; no Android dependency. */
final class ErrorLogStore {
    static final int MAX_SESSIONS=12;
    static final long LOG_BYTES=1024L*1024L;
    static synchronized void append(File file,String text,long limit)throws IOException {
        byte[] bytes=text.getBytes(StandardCharsets.UTF_8);
        if(file.length()+bytes.length>limit){
            File old=new File(file.getPath()+".previous");
            if(old.exists()&&!old.delete())throw new IOException("Cannot rotate error log");
            if(file.exists()&&!file.renameTo(old))throw new IOException("Cannot rotate error log");
        }
        try(OutputStream out=new FileOutputStream(file,true)){out.write(bytes,0,(int)Math.min(bytes.length,limit));}
    }
    static File[] sessions(File root){
        File[] dirs=root.listFiles(File::isDirectory);if(dirs==null)return new File[0];
        Arrays.sort(dirs,Comparator.comparing(File::getName).reversed());
        return Arrays.copyOf(dirs,Math.min(dirs.length,MAX_SESSIONS));
    }
    static void prune(File root)throws IOException {
        File[] dirs=root.listFiles(File::isDirectory);if(dirs==null)return;
        Arrays.sort(dirs,Comparator.comparing(File::getName).reversed());
        for(int i=MAX_SESSIONS;i<dirs.length;i++)delete(dirs[i]);
    }
    private static void delete(File file)throws IOException {
        if(file.isDirectory()){File[] children=file.listFiles();if(children!=null)for(File child:children)delete(child);}
        if(!file.delete())throw new IOException("Cannot remove old error session");
    }
    static long copyBounded(InputStream in,OutputStream out,long limit)throws IOException {
        byte[] buffer=new byte[8192];long total=0;int n;
        while(total<limit&&(n=in.read(buffer,0,(int)Math.min(buffer.length,limit-total)))!=-1){out.write(buffer,0,n);total+=n;}
        return total;
    }
    private static boolean allowed(String name){
        return name.equals("metadata.json")||name.equals("lifecycle.log")||name.equals("lifecycle.log.previous")
            ||name.equals("logcat.log")||name.equals("logcat.log.previous")
            ||name.equals("engine.log")||name.equals("engine.log.previous")
            ||name.equals("native-maps.txt")||name.equals("native-signal.txt")||name.equals("java-exception.txt")
            ||name.matches("exit-[0-9]+-[0-9]+\\.(json|trace\\.pb|trace\\.txt)");
    }
    static void export(OutputStream stream,File[] sessions)throws IOException {
        try(ZipOutputStream zip=new ZipOutputStream(new BufferedOutputStream(stream))){
            zip.putNextEntry(new ZipEntry("ABOUT.txt"));
            zip.write(("Carmageddon Android Port opt-in error capture.\n"
                +"A missing/unfinished session does not prove a crash. Android may omit exit traces.\n"
                +"Native traces ending .pb are Android tombstone protobufs; .txt traces are text.\n"
                +"Logs are size-limited, may include app messages/local paths, and are never uploaded automatically.\n").getBytes(StandardCharsets.UTF_8));zip.closeEntry();
            for(File dir:sessions){File[] files=dir.listFiles();if(files==null)continue;
                Arrays.sort(files,Comparator.comparing(File::getName));
                for(File file:files)if(file.isFile()&&allowed(file.getName())){
                    if(!file.getCanonicalPath().startsWith(dir.getCanonicalPath()+File.separator))continue;
                    zip.putNextEntry(new ZipEntry(dir.getName()+"/"+file.getName()));
                    try(InputStream in=new FileInputStream(file)){copyBounded(in,zip,2L*1024*1024);}
                    zip.closeEntry();
                }
            }
        }
    }
    private ErrorLogStore(){}
}
