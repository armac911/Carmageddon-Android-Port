package com.dethrace.android;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Structural fixtures only: no original game content. */
public final class GameContentTest {
    static void check(boolean value){if(!value)throw new AssertionError();}
    static byte[] zip(boolean splat,boolean full)throws Exception {
        ByteArrayOutputStream data=new ByteArrayOutputStream();
        try(ZipOutputStream out=new ZipOutputStream(data)){
            List<String> files=new ArrayList<>(Arrays.asList("data/general.txt","data/keymap_0.txt","data/cars/","music/Track02.ogg"));
            if(splat){files.add("data/races/castle.txt");if(full)files.add("data/races/castle2.txt");}
            else files.add("data/races/citya1.txt");
            for(String name:files){out.putNextEntry(new ZipEntry(name));if(!name.endsWith("/"))out.write((splat?"splat":"original").getBytes());out.closeEntry();}
        }return data.toByteArray();
    }
    static void rejected(File root,String edition,byte[] bytes)throws Exception {
        try {GameContent.install(new ByteArrayInputStream(bytes),root,edition);throw new AssertionError("Accepted invalid import");}
        catch(IOException expected){}
        check(!new File(root,"import-staging-"+edition).exists());
    }
    public static void main(String[] args)throws Exception {
        File root=Files.createTempDirectory("carma-content-").toFile();
        try {
            check(GameContent.ORIGINAL.equals(GameContent.normalize(null)));
            check(GameContent.ORIGINAL.equals(GameContent.normalize("bad")));
            check(GameContent.directory(root,GameContent.ORIGINAL).equals(new File(root,"game")));
            check(GameContent.directory(root,GameContent.SPLAT).equals(new File(root,"splatpack")));
            GameContent.install(new ByteArrayInputStream(zip(false,true)),root,GameContent.ORIGINAL);
            File original=GameContent.directory(root,GameContent.ORIGINAL);
            File save=new File(original,"save-sentinel");Files.write(save.toPath(),"keep-original-save".getBytes());
            byte[] before=Files.readAllBytes(new File(original,"DATA/GENERAL.TXT").toPath());
            rejected(root,GameContent.SPLAT,zip(false,true)); // original ZIP is not expansion data
            rejected(root,GameContent.SPLAT,zip(true,false)); // expansion demo is not full Splat Pack
            check(!new File(root,"splatpack").exists());
            GameContent.install(new ByteArrayInputStream(zip(true,true)),root,GameContent.SPLAT);
            File expansion=GameContent.directory(root,GameContent.SPLAT);
            check(GameDataZip.isReady(original,GameContent.ORIGINAL));check(!GameDataZip.isReady(original,GameContent.SPLAT));
            check(GameDataZip.isReady(expansion,GameContent.SPLAT));check(!GameDataZip.isReady(expansion,GameContent.ORIGINAL));
            check(new File(expansion,"MUSIC/Track02.ogg").isFile());
            check(Arrays.equals(before,Files.readAllBytes(new File(original,"DATA/GENERAL.TXT").toPath())));
            check(new String(Files.readAllBytes(save.toPath())).equals("keep-original-save"));
            rejected(root,GameContent.ORIGINAL,zip(true,true)); // existing installation cannot be replaced
            rejected(root,GameContent.SPLAT,zip(true,true));
            File empty=new File(root,"fresh");empty.mkdir();rejected(empty,GameContent.ORIGINAL,zip(true,true));
            check(!new File(empty,"game").exists());
            try {GameContent.install(new ByteArrayInputStream(zip(false,true)),empty,"bad");throw new AssertionError();}catch(IOException expected){}
            File raceRoot=new File(root,"concurrent");raceRoot.mkdir();byte[] expansionZip=zip(true,true);
            java.util.concurrent.CountDownLatch gate=new java.util.concurrent.CountDownLatch(1);
            java.util.concurrent.atomic.AtomicInteger installed=new java.util.concurrent.atomic.AtomicInteger();
            List<Thread> workers=new ArrayList<>();
            for(int i=0;i<4;i++){Thread worker=new Thread(()->{try{gate.await();GameContent.install(new ByteArrayInputStream(expansionZip),raceRoot,GameContent.SPLAT);installed.incrementAndGet();}catch(Exception expected){}});workers.add(worker);worker.start();}
            gate.countDown();for(Thread worker:workers)worker.join();
            check(installed.get()==1&&GameDataZip.isReady(GameContent.directory(raceRoot,GameContent.SPLAT),GameContent.SPLAT));
            check(!new File(raceRoot,"import-staging-splat").exists());
            System.out.println("PASS serialized imports, separate game roots, save preservation, transactional imports, wrong-edition/demo rejection, selection fallback and existing-data protection");
        }finally{GameDataZip.deleteTree(root);}
    }
}
