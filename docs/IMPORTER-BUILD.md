# Native GOG installer reader

`android/scripts/setup-importer.sh` builds a native ARM64 Android PIE named
`libinnoextract.so` and places it under `android/app/libs/arm64-v8a/` for APK
packaging. `build-dev.sh` invokes it automatically. Standalone Gradle builds
require this setup first. The APK extracts native libraries into its installed,
read-only executable library directory; downloaded EXE files are read as data.
No executable is downloaded or made executable at runtime.

Requirements: JDK 17, NDK 28.2.13676358, CMake 3.22.1+, a host C/C++ build tool,
and network access for the pinned source archives. Use `ANDROID_NDK_HOME` and
`CARMA_CMAKE` to override tool paths. The usual Android build also needs the SDK,
Gradle and SDL described in the main README. Windows native-helper builds require
an equivalent CMake invocation and copying its output to the same `libs` path;
that host workflow has not been validated.

The dependency versions and SHA-256 values are fixed in
`android/installer/CMakeLists.txt`:

- Daniel Scharrer's innoextract, commit `6e9e34ed0876014fdb46e684103ef8c3605e382e`.
- Boost 1.85.0, static filesystem/iostreams/program-options dependencies.
- XZ 5.4.6 **liblzma** and bzip2 1.0.8, statically linked.
- Android's platform zlib; no extra shared C++ runtime is required.

The upstream innoextract build file is copied into the build directory and
patched only to consume these bundled dependency targets. The parser/extractor
source is unchanged. The source URLs, hashes and integration patch are included
in the repository; retained dependency notices are under `docs/licenses/`.

The launcher detects ZIP/EXE signatures, stages input in app-owned storage, and
invokes the reader with an argument array (no shell). Only DATA and MUSIC path
components are extracted. The common importer then selects one game and applies
its support overlay before atomic installation. Extraction has a five-minute,
2 GiB, 50,000-file and nesting limit, with bounded diagnostic output. ZIP path,
case-collision and extracted-byte checks are retained. Allow free space for the
input copy, extracted installer contents and final staged game together.

Self-contained Inno Setup installers are the initial supported EXE route.
Multipart BIN/RAR installers, password-protected installers, ZIP-contained EXE
installers, disk images and other installer technologies are not supported.
The exact GOG Carmageddon installer still needs a real-file/device test.
