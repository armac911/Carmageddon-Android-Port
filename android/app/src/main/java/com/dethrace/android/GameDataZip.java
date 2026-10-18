package com.dethrace.android;

import java.io.*;
import java.util.Locale;
import java.util.HashSet;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Imports only DATA and MUSIC from a user-prepared ZIP into a fresh directory. */
public final class GameDataZip {
    private static final long MAX_BYTES = 2L * 1024 * 1024 * 1024;
    private static final int MAX_ENTRIES = 50000;

    private GameDataZip() {}

    public static boolean isReady(File root) {
        return new File(root, "DATA/GENERAL.TXT").isFile()
            && new File(root, "DATA/KEYMAP_0.TXT").isFile()
            && new File(root, "DATA/CARS").isDirectory()
            && new File(root, "DATA/RACES/CITYA1.TXT").isFile();
    }

    public static void extract(InputStream input, File destination) throws IOException {
        if (destination.exists()) throw new IOException("Import destination already exists");
        if (!destination.mkdirs()) throw new IOException("Cannot create import directory");
        long total = 0;
        int entries = 0;
        Set<String> seen = new HashSet<>();
        byte[] buffer = new byte[65536];
        try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(input))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (++entries > MAX_ENTRIES) throw new IOException("Too many ZIP entries");
                String name = entry.getName().replace('\\', '/');
                if (name.startsWith("/") || name.contains(":")) throw new IOException("Invalid ZIP path");
                for (String part : name.split("/")) {
                    if (part.equals("..")) throw new IOException("Invalid ZIP path");
                }
                String upper = name.toUpperCase(Locale.ROOT);
                // Require the documented layout; never guess between CARMA and CARSPLAT.
                if (!(upper.startsWith("DATA/") || upper.startsWith("MUSIC/")))
                    throw new IOException("ZIP must contain DATA/ and optionally MUSIC/ at its root");
                if (!seen.add(upper)) throw new IOException("Duplicate ZIP path: " + name);
                // DATA uses case-insensitive DOS paths; normalize for native access() calls.
                String normalized = upper.startsWith("DATA/") ? upper
                    : "MUSIC/" + name.substring(6);
                File target = new File(destination, normalized);
                if (!target.getCanonicalPath().startsWith(destination.getCanonicalPath() + File.separator))
                    throw new IOException("Invalid ZIP path");
                if (entry.isDirectory()) {
                    if (!target.isDirectory() && !target.mkdirs()) throw new IOException("Cannot create directory");
                } else {
                    File parent = target.getParentFile();
                    if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create directory");
                    try (OutputStream out = new BufferedOutputStream(new FileOutputStream(target))) {
                        int count;
                        while ((count = zip.read(buffer)) != -1) {
                            total += count;
                            if (total > MAX_BYTES) throw new IOException("Game data exceeds 2 GiB limit");
                            out.write(buffer, 0, count);
                        }
                    }
                }
                zip.closeEntry();
            }
        }
        if (!isReady(destination)) throw new IOException(
            "Incomplete original PC game data: need DATA/GENERAL.TXT, KEYMAP_0.TXT, CARS/ and RACES/CITYA1.TXT. "
            + "For GOG, merge support DATA files before making the ZIP.");
    }

    public static void deleteTree(File file) throws IOException {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot list " + file.getName());
            for (File child : children) deleteTree(child);
        }
        if (!file.delete()) throw new IOException("Cannot remove " + file.getName());
    }
}
