package com.dethrace.android;

import java.io.*;
import java.util.*;
import java.util.zip.*;

/** Select one complete game from a ZIP or an unpacked installer, then merge its support files. */
public final class GameDataZip {
    static final long MAX_BYTES = 2L * 1024 * 1024 * 1024;
    private static final int MAX_ENTRIES = 50000;
    private GameDataZip() {}
    public static boolean isReady(File root) {return isReady(root,GameContent.ORIGINAL);}
    public static boolean isReady(File root,String edition) {
        boolean common=new File(root,"DATA/GENERAL.TXT").isFile()
            &&new File(root,"DATA/KEYMAP_0.TXT").isFile()&&new File(root,"DATA/CARS").isDirectory();
        if(!common)return false;
        if(GameContent.SPLAT.equals(edition))return new File(root,"DATA/RACES/CASTLE.TXT").isFile()
            &&new File(root,"DATA/RACES/CASTLE2.TXT").isFile();
        return new File(root,"DATA/RACES/CITYA1.TXT").isFile()
            &&!new File(root,"DATA/RACES/CASTLE.TXT").exists();
    }
    private interface Source { InputStream open(String name)throws IOException; }
    private static String safePath(String name)throws IOException {
        name=name.replace('\\','/');
        if(name.startsWith("/")||name.contains(":")||name.indexOf('\0')>=0)throw new IOException("Invalid archive path");
        StringBuilder result=new StringBuilder();
        for(String part:name.split("/")) {
            if(part.equals(".."))throw new IOException("Invalid archive path");
            if(part.isEmpty()||part.equals("."))continue;
            if(result.length()>0)result.append('/');result.append(part);
        }
        return result.toString();
    }
    public static void extract(InputStream input,File destination)throws IOException {extract(input,destination,GameContent.ORIGINAL);}
    public static void extract(InputStream input,File destination,String edition)throws IOException {
        File parent=destination.getAbsoluteFile().getParentFile();
        if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create import staging");
        File archive=File.createTempFile("carma-archive-",".zip",parent);
        try {
            try(OutputStream out=new BufferedOutputStream(new FileOutputStream(archive))) {copy(input,out,MAX_BYTES);}
            extract(archive,destination,edition);
        }finally{if(!archive.delete()&&archive.exists())archive.deleteOnExit();}
    }
    public static void extract(File archive,File destination,String edition)throws IOException {
        try(ZipFile zip=new ZipFile(archive)) {
            Map<String,String> files=new LinkedHashMap<>();int count=0;
            Enumeration<? extends ZipEntry> entries=zip.entries();
            while(entries.hasMoreElements()) {
                ZipEntry entry=entries.nextElement();
                if(++count>MAX_ENTRIES)throw new IOException("Too many archive entries");
                String path=safePath(entry.getName());
                {
                    String key=path.toUpperCase(Locale.ROOT)+(entry.isDirectory()?"/":"");
                    if(files.put(key,entry.getName())!=null)throw new IOException("Duplicate archive path: "+path);
                }
            }
            extract(files,name->zip.getInputStream(zip.getEntry(name)),destination,edition);
        }
    }
    static void extractDirectory(File source,File destination,String edition)throws IOException {
        Map<String,String> files=new LinkedHashMap<>();
        collect(source,source,files);
        extract(files,name->new FileInputStream(new File(source,name)),destination,edition);
    }
    private static void collect(File root,File dir,Map<String,String> files)throws IOException {
        File[] children=dir.listFiles();if(children==null)throw new IOException("Cannot read unpacked installer");
        for(File f:children) {
            String canonical=f.getCanonicalPath(), expected=new File(dir.getCanonicalFile(),f.getName()).getPath();
            if(!canonical.equals(expected)||!canonical.startsWith(root.getCanonicalPath()+File.separator))throw new IOException("Unsafe installer path");
            if(f.isDirectory())collect(root,f,files);
            else {
                String path=safePath(root.toURI().relativize(f.toURI()).getPath());
                if(files.put(path.toUpperCase(Locale.ROOT),path)!=null)throw new IOException("Duplicate installer path");
                if(files.size()>MAX_ENTRIES)throw new IOException("Too many installer files");
            }
        }
    }
    private static void extract(Map<String,String> files,Source source,File destination,String edition)throws IOException {
        if(!GameContent.ORIGINAL.equals(edition)&&!GameContent.SPLAT.equals(edition))throw new IOException("Unknown game selection");
        if(destination.exists())throw new IOException("Import destination already exists");
        List<String> candidates=new ArrayList<>();
        for(String path:files.keySet()) {
            if(!path.endsWith("DATA/GENERAL.TXT"))continue;
            String prefix=path.substring(0,path.length()-"DATA/GENERAL.TXT".length());
            if(!prefix.isEmpty()&&!prefix.endsWith("/"))continue;
            if(prefix.contains("__SUPPORT/"))continue;
            boolean splat=files.containsKey(prefix+"DATA/RACES/CASTLE.TXT");
            boolean matches=GameContent.SPLAT.equals(edition)
                ?splat&&files.containsKey(prefix+"DATA/RACES/CASTLE2.TXT")
                :!splat&&files.containsKey(prefix+"DATA/RACES/CITYA1.TXT");
            if(matches)candidates.add(prefix);
        }
        if(candidates.isEmpty())throw new IOException("Cannot find complete "+GameContent.label(edition)+" DATA in this file. Select the matching game and supply the full PC data.");
        if(candidates.size()!=1)throw new IOException("Multiple copies of "+GameContent.label(edition)+" found. Choose an archive containing one installation.");
        String root=candidates.get(0),leaf=root.isEmpty()?"":root.substring(0,root.length()-1);
        leaf=leaf.substring(leaf.lastIndexOf('/')+1);
        String gameName=GameContent.SPLAT.equals(edition)?"CARSPLAT":"CARMA";
        List<String> support=new ArrayList<>();
        // GOG Max Pack: sibling __support/app/CARMA or CARSPLAT. Only the selected game's support tree.
        String packageRoot=root.isEmpty()?"":leaf.equals(gameName)?root.substring(0,root.length()-leaf.length()-1):root;
        String supportRoot=packageRoot+"__SUPPORT/APP/"+gameName+"/";
        for(String path:files.keySet())if(path.startsWith(supportRoot+"DATA/")){support.add(supportRoot);break;}
        if(!destination.mkdirs())throw new IOException("Cannot create import directory");
        long remaining=MAX_BYTES;
        Map<String,Boolean> layers=new LinkedHashMap<>();layers.put(root,true);
        // Prefer MUSIC beside the selected DATA; otherwise look in enclosing
        // folders (common installer layouts), never inside the other game.
        String musicRoot=root;
        while(true) {
            boolean found=false;
            for(String path:files.keySet())if(path.startsWith(musicRoot+"MUSIC/")&&!path.endsWith("/")){found=true;break;}
            if(found){if(!musicRoot.equals(root))layers.put(musicRoot,false);break;}
            if(musicRoot.isEmpty())break;
            String parent=musicRoot.substring(0,musicRoot.length()-1);
            int slash=parent.lastIndexOf('/');musicRoot=slash<0?"":parent.substring(0,slash+1);
        }
        for(String layer:support)layers.put(layer,true);
        for(Map.Entry<String,Boolean> selected:layers.entrySet())for(Map.Entry<String,String> entry:files.entrySet()) {
            String layer=selected.getKey();
            String path=entry.getKey();if(!path.startsWith(layer))continue;
            String relative=path.substring(layer.length());
            if(!(selected.getValue()&&relative.startsWith("DATA/"))&&!relative.startsWith("MUSIC/"))continue;
            String output=relative.startsWith("MUSIC/")?"MUSIC"+safePath(entry.getValue()).substring(layer.length()+5)+(relative.endsWith("/")?"/":""):relative;
            File target=new File(destination,output);
            if(!target.getCanonicalPath().startsWith(destination.getCanonicalPath()+File.separator))throw new IOException("Invalid output path");
            if(relative.endsWith("/")){if(!target.isDirectory()&&!target.mkdirs())throw new IOException("Cannot create folder");continue;}
            File parent=target.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Cannot create data folder");
            try(InputStream in=source.open(entry.getValue());OutputStream out=new BufferedOutputStream(new FileOutputStream(target))) {
                remaining-=copy(in,out,remaining);
            }
        }
        if(!isReady(destination,edition))throw new IOException("Incomplete "+GameContent.label(edition)+" data: missing KEYMAP_0.TXT, CARS or race files. Include the matching GOG support files when present.");
    }
    static long copy(InputStream in,OutputStream out,long limit)throws IOException {
        long total=0;byte[] buffer=new byte[65536];int n;
        while((n=in.read(buffer))!=-1) {total+=n;if(total>limit)throw new IOException("Import exceeds 2 GiB limit");out.write(buffer,0,n);}
        return total;
    }
    public static void deleteTree(File file)throws IOException {
        if(!file.exists())return;
        if(file.isDirectory()) {
            File[] children=file.listFiles();if(children==null)throw new IOException("Cannot list "+file.getName());
            for(File child:children)deleteTree(child);
        }
        if(!file.delete())throw new IOException("Cannot remove "+file.getName());
    }
}
