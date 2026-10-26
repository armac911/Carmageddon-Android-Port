package com.dethrace.android;
import java.io.*;
import java.nio.file.*;
public class GogImportTest {
 static void check(boolean x){if(!x)throw new AssertionError();}
 static InputStream installer(){return new ByteArrayInputStream(new byte[]{'M','Z',0,0});}
 public static void main(String[] args)throws Exception {
  File root=Files.createTempDirectory("gog-import").toFile();
  try {
   try{GameContent.install(installer(),root,GameContent.ORIGINAL);throw new AssertionError();}catch(IOException expected){check(expected.getMessage().contains("unavailable"));}
   check(!GameContent.directory(root,GameContent.ORIGINAL).exists());
   // Controlled helper fixture checks routing, argv and transactional promotion.
   File helper=new File(root,"helper");
   String script="#!/bin/sh\nwhile [ \"$1\" != \"--output-dir\" ]; do shift; done\nshift\nout=$1\nmkdir -p \"$out/app/CARMA/DATA/CARS\" \"$out/app/CARMA/DATA/RACES\" \"$out/app/__support/app/CARMA/DATA\" \"$out/app/CARMA/MUSIC\"\necho data > \"$out/app/CARMA/DATA/GENERAL.TXT\"\necho car > \"$out/app/CARMA/DATA/CARS/A.TXT\"\necho race > \"$out/app/CARMA/DATA/RACES/CITYA1.TXT\"\necho keys > \"$out/app/__support/app/CARMA/DATA/KEYMAP_0.TXT\"\necho music > \"$out/app/CARMA/MUSIC/Track02.ogg\"\n";
   Files.write(helper.toPath(),script.getBytes("UTF-8"));check(helper.setExecutable(true));
   GameContent.install(installer(),root,GameContent.ORIGINAL,helper);
   check(GameDataZip.isReady(GameContent.directory(root,GameContent.ORIGINAL)));
   check(!new File(root,"installer-staging-original").exists());
   for(File f:root.listFiles())check(!f.getName().startsWith("carma-gog-"));
   File saves=new File(GameContent.directory(root,GameContent.ORIGINAL),"SAVE.SAV");Files.write(saves.toPath(),new byte[]{42});
   if(args.length>0) {
    try{GameContent.install(installer(),root,GameContent.SPLAT,new File(args[0]));throw new AssertionError();}catch(IOException expected){check(expected.getMessage().contains("Cannot unpack"));}
    check(!GameContent.directory(root,GameContent.SPLAT).exists());
    check(!new File(root,"installer-staging-splat").exists());
   }
   check(Files.readAllBytes(saves.toPath())[0]==42);
   System.out.println("PASS installer content detection, bundled-reader routing, support merge, transactional cleanup and rejection without save loss");
  }finally{GameDataZip.deleteTree(root);}
 }
}
