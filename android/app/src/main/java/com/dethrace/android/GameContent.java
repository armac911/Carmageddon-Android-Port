package com.dethrace.android;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;

/** Separate roots preserve the original installation and each game's saves. */
final class GameContent {
    static final String ORIGINAL="original", SPLAT="splat";
    static String normalize(String edition){return SPLAT.equals(edition)?SPLAT:ORIGINAL;}
    static String label(String edition){return SPLAT.equals(edition)?"Splat Pack":"Carmageddon";}
    static File directory(File root,String edition){return new File(root,SPLAT.equals(edition)?"splatpack":"game");}
    static synchronized void install(InputStream input,File appRoot,String edition)throws IOException {
        if(!ORIGINAL.equals(edition)&&!SPLAT.equals(edition))throw new IOException("Unknown game selection");
        File destination=directory(appRoot,edition);
        if(destination.exists())throw new IOException(label(edition)+" data is already installed; existing data and saves are preserved.");
        File staging=new File(appRoot,"import-staging-"+edition);
        try {
            GameDataZip.deleteTree(staging);
            GameDataZip.extract(input,staging,edition);
            if(destination.exists()||!staging.renameTo(destination))throw new IOException("Cannot install "+label(edition)+" data directory");
        }catch(IOException|RuntimeException e){
            try{GameDataZip.deleteTree(staging);}catch(IOException cleanup){e.addSuppressed(cleanup);}
            throw e;
        }
    }
    private GameContent(){}
}
