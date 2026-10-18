import com.dethrace.android.GameDataZip;
import java.io.*;
import java.nio.file.Files;
import java.util.zip.*;

/** Standalone JVM regression checks; uses generated markers, no game assets. */
public class GameDataZipTest {
    private static byte[] archive(String... names) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            for (String name : names) {
                zip.putNextEntry(new ZipEntry(name));
                if (!name.endsWith("/")) zip.write(new byte[] {1, 2, 3});
                zip.closeEntry();
            }
        }
        return bytes.toByteArray();
    }

    private static void reject(File parent, String name, String... entries) throws Exception {
        File dest = new File(parent, name);
        try {
            GameDataZip.extract(new ByteArrayInputStream(archive(entries)), dest);
            throw new AssertionError("Accepted invalid archive: " + name);
        } catch (IOException expected) {
            if (GameDataZip.isReady(dest)) throw new AssertionError("Invalid import ready");
        } finally { GameDataZip.deleteTree(dest); }
    }

    public static void main(String[] args) throws Exception {
        File temp = Files.createTempDirectory("carma-zip-test-").toFile();
        try {
            File valid = new File(temp, "valid");
            GameDataZip.extract(new ByteArrayInputStream(archive(
                "data/general.txt", "data/keymap_0.txt", "data/cars/", "data/races/citya1.txt",
                "music/Track02.ogg")), valid);
            if (!GameDataZip.isReady(valid)) throw new AssertionError("Valid structure rejected");
            if (!new File(valid, "MUSIC/Track02.ogg").isFile()) throw new AssertionError("Music case lost");
            byte[] before = Files.readAllBytes(new File(valid, "DATA/GENERAL.TXT").toPath());
            try {
                GameDataZip.extract(new ByteArrayInputStream(archive("DATA/GENERAL.TXT")), valid);
                throw new AssertionError("Existing data overwritten");
            } catch (IOException expected) {}
            if (!java.util.Arrays.equals(before, Files.readAllBytes(new File(valid, "DATA/GENERAL.TXT").toPath())))
                throw new AssertionError("Existing data changed");
            reject(temp, "traversal", "DATA/../../outside.txt");
            reject(temp, "absolute", "/DATA/GENERAL.TXT");
            reject(temp, "windows", "C:\\DATA\\GENERAL.TXT");
            reject(temp, "wrapper", "CARMA/DATA/GENERAL.TXT");
            reject(temp, "duplicate", "DATA/GENERAL.TXT", "data/general.txt");
            reject(temp, "incomplete", "DATA/GENERAL.TXT");
            if (new File(temp, "outside.txt").exists()) throw new AssertionError("Escaped staging directory");
            System.out.println("PASS: valid import, case normalization, music case, existing-data preservation, traversal, absolute/Windows paths, wrapped layout, duplicates, incomplete data");
        } finally { GameDataZip.deleteTree(temp); }
    }
}
