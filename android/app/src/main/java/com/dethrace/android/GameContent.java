package com.dethrace.android;

import java.io.File;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.IOException;

/** Separate roots preserve the original installation and each game's saves. */
final class GameContent {
    static final String ORIGINAL="original", SPLAT="splat";
    static String normalize(String edition){return SPLAT.equals(edition)?SPLAT:ORIGINAL;}
    static String label(String edition){return SPLAT.equals(edition)?"Splat Pack":"Carmageddon";}
    static File directory(File root,String edition){return new File(root,SPLAT.equals(edition)?"splatpack":"game");}
    static synchronized void install(InputStream input,File appRoot,String edition)throws IOException {
        install(input,appRoot,edition,null);
    }
    static synchronized void install(InputStream input,File appRoot,String edition,File extractor)throws IOException {
        if(!ORIGINAL.equals(edition)&&!SPLAT.equals(edition))throw new IOException("Unknown game selection");
        File destination=directory(appRoot,edition);
        if(destination.exists())throw new IOException(label(edition)+" data is already installed; existing data and saves are preserved.");
        // Remove only our abandoned input copies after a prior interrupted import.
        // The synchronized installer guarantees no other import is reading them.
        File[] oldInputs=appRoot.listFiles();
        if(oldInputs!=null)for(File f:oldInputs)if(f.isFile()&&
            ((f.getName().startsWith("carma-gog-")&&f.getName().endsWith(".exe"))||
             (f.getName().startsWith("carma-archive-")&&f.getName().endsWith(".zip"))))
            if(!f.delete())throw new IOException("Cannot clean abandoned import input");
        File staging=new File(appRoot,"import-staging-"+edition);
        File unpacked=new File(appRoot,"installer-staging-"+edition);
        try {
            GameDataZip.deleteTree(staging);GameDataZip.deleteTree(unpacked);
            BufferedInputStream buffered=new BufferedInputStream(input);buffered.mark(4);
            int first=buffered.read(),second=buffered.read();buffered.reset();
            if(first=='M'&&second=='Z') {
                GogInstaller.extract(buffered,unpacked,extractor);
                GameDataZip.extractDirectory(unpacked,staging,edition);
                GameDataZip.deleteTree(unpacked);
            }else if(first=='P'&&second=='K')GameDataZip.extract(buffered,staging,edition);
            else throw new IOException("Choose a game ZIP or supported GOG offline installer EXE");
            if(destination.exists()||!staging.renameTo(destination))throw new IOException("Cannot install "+label(edition)+" data directory");
        }catch(IOException|RuntimeException e){
            try{GameDataZip.deleteTree(staging);GameDataZip.deleteTree(unpacked);}catch(IOException cleanup){e.addSuppressed(cleanup);}
            throw e;
        }
    }
    private GameContent(){}
}
