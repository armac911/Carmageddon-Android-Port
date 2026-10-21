package com.dethrace.android;
import java.io.*;
import java.nio.file.*;
import java.util.zip.*;
public class AutoImportTest {
    static byte[] zip(String... files)throws Exception {
        ByteArrayOutputStream b=new ByteArrayOutputStream();
        try(ZipOutputStream z=new ZipOutputStream(b)){for(String name:files){z.putNextEntry(new ZipEntry(name));z.write(name.getBytes("UTF-8"));z.closeEntry();}}return b.toByteArray();
    }
    static String[] full(String root,boolean splat){return new String[]{root+"DATA/GENERAL.TXT",root+"DATA/KEYMAP_0.TXT",root+"DATA/CARS/TEST.TXT",root+"DATA/RACES/"+(splat?"CASTLE":"CITYA1")+".TXT",root+"DATA/RACES/"+(splat?"CASTLE2":"CITYB3")+".TXT"};}
    static String[] join(String[] a,String... b){String[] c=java.util.Arrays.copyOf(a,a.length+b.length);System.arraycopy(b,0,c,a.length,b.length);return c;}
    static void check(boolean x){if(!x)throw new AssertionError();}
    public static void main(String[] args)throws Exception {
        File tmp=Files.createTempDirectory("auto-import").toFile();
        try {
            String[] original=full("Max Pack/CARMA/",false),splat=full("Max Pack/CARSPLAT/",true);
            String[] entries=join(join(original,splat),"Max Pack/__support/app/CARMA/DATA/KEYMAP_0.TXT","Max Pack/__support/app/CARSPLAT/DATA/KEYMAP_0.TXT","Max Pack/CARMA/MUSIC/","Max Pack/CARMA/MUSIC/Track02.ogg","Max Pack/CARSPLAT/MUSIC/Track03.ogg","Max Pack/dosbox.exe","manual.pdf");
            File a=new File(tmp,"a"),b=new File(tmp,"b");
            GameDataZip.extract(new ByteArrayInputStream(zip(entries)),a,GameContent.ORIGINAL);
            GameDataZip.extract(new ByteArrayInputStream(zip(entries)),b,GameContent.SPLAT);
            check(GameDataZip.isReady(a)&&GameDataZip.isReady(b,GameContent.SPLAT));
            check(new String(Files.readAllBytes(new File(a,"DATA/KEYMAP_0.TXT").toPath()),"UTF-8").contains("__support/app/CARMA"));
            check(new File(a,"MUSIC/Track02.ogg").isFile()&&!new File(a,"MUSIC/Track03.ogg").exists());
            check(!new File(a,"DATA/RACES/CASTLE.TXT").exists()&&!new File(a,"dosbox.exe").exists());
            // Missing base keymap is repaired by matching support files.
            String[] noKey={"app/CARMA/DATA/GENERAL.TXT","app/CARMA/DATA/CARS/A.TXT","app/CARMA/DATA/RACES/CITYA1.TXT","app/__support/app/CARMA/DATA/KEYMAP_0.TXT","app/MUSIC/TRACK04.OGG"};
            File c=new File(tmp,"c");GameDataZip.extract(new ByteArrayInputStream(zip(noKey)),c);
            check(GameDataZip.isReady(c)&&new File(c,"MUSIC/TRACK04.OGG").isFile());
            try{GameDataZip.extract(new ByteArrayInputStream(zip(join(full("one/",false),full("two/",false)))),new File(tmp,"ambiguous"));throw new AssertionError();}catch(IOException expected){}
            // Installer directory and ZIP follow the same selection/overlay path.
            File unpacked=new File(tmp,"unpacked");for(String path:noKey){File f=new File(unpacked,path);f.getParentFile().mkdirs();Files.write(f.toPath(),path.getBytes("UTF-8"));}
            File d=new File(tmp,"d");GameDataZip.extractDirectory(unpacked,d,GameContent.ORIGINAL);check(GameDataZip.isReady(d));
            System.out.println("PASS automatic nested ZIP/installer selection, support overlays, music, edition isolation and ambiguity rejection");
        }finally{GameDataZip.deleteTree(tmp);}
    }
}
