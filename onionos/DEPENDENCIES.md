# Dependencies and reproducible build

The engine's existing LICENSE (GPL-3.0) and THIRD-PARTY-NOTICES.md apply.
No original Carmageddon assets may be packaged.

## Versions used for experimental 0.1.4

| Component | Version/source |
| --- | --- |
| Engine base | armac911/Carmageddon-Android-Port `c4fadaca8aea901ee93d854a806a44334e2674da` |
| Branch | `experimental/onionos-miyoo` |
| Host CMake | 3.16.9 from Mini SDK |
| Cross compiler | arm-linux-gnueabihf GCC 8.2.1, Cortex-A7 / NEON-VFPv4 / hard-float |
| Buildroot SDK | [mini_toolchain-v1.0.tar.gz](https://github.com/steward-fu/website/releases/download/miyoo-mini/mini_toolchain-v1.0.tar.gz) |
| SDL2 Mini | [steward-fu/sdl2](https://github.com/steward-fu/sdl2), `0631abc8e8916db6f9bc7e2afd0c22913d092a29`, modified by `prepare-sdl.py` |
| json-c | 0.15 from SDK, MIT; license included |
| BRender, libsmacker, miniaudio | Existing repository versions, no additional downloads |
| MI_* and shmvar | Existing device firmware; link-time SDK .so files, not redistributed |

SDL2 upstream portions use zlib licensing. The Mini adapter is LGPL-2.1; the
ZIP includes exact tracked SDL source with the two modified C files and
configure.ac, plus licenses. It excludes unrelated dependency tarballs, Android
sample project and tests, and proprietary vendor .so binaries. Dynamically linked
SDL can be replaced in the app-local lib directory. The engine source archive
in the ZIP is made from the committed experimental branch, not an Android
release archive. The source URL/commit are also recorded in BUILD-INFO.txt.

## Prepare toolchain and SDL

Use an x86_64 Linux host (or a Linux Docker container). Install standard host
build tools: make, CMake, autoconf, automake, libtool, pkg-config, Python 3, Git.
Download the SDK above and extract `mini` and `prebuilt` under `/opt`. Do not copy
these build tools to the handheld. Keep all dependency builds outside the game
repository.

```sh
export PATH=/opt/mini/bin:/opt/prebuilt/bin:$PATH
git clone https://github.com/steward-fu/sdl2.git /tmp/miyoo-sdl2
git -C /tmp/miyoo-sdl2 checkout 0631abc8e8916db6f9bc7e2afd0c22913d092a29
python3 onionos/prepare-sdl.py /tmp/miyoo-sdl2
cd /tmp/miyoo-sdl2/sdl2
./autogen.sh
./configure --host=arm-linux-gnueabihf --prefix=/tmp/miyoo-sdl-install \
  CC=/opt/mini/bin/arm-linux-gnueabihf-gcc \
  --disable-oss --disable-alsa --disable-video-opengl \
  --disable-video-opengles --disable-video-opengles2 \
  --enable-audio-mini --enable-video-mini \
  --disable-video-x11 --disable-video-wayland --disable-video-kmsdrm \
  --disable-video-vulkan --disable-pulseaudio --disable-jack \
  --disable-pipewire --disable-libudev --disable-dbus --disable-hidapi \
  --disable-ibus --disable-fcitx
make -j4
make install
```

Return to the engine checkout, then:

```sh
SDL_PREFIX=/tmp/miyoo-sdl-install \
MIYOO_SDK_LIB_DIR=/tmp/miyoo-sdl2/mini/lib \
CMAKE=/opt/mini/bin/cmake sh onionos/build.sh
python3 onionos/package.py \
  --build build-miyoo --sdl-prefix /tmp/miyoo-sdl-install \
  --sysroot /opt/mini/arm-buildroot-linux-gnueabihf/sysroot \
  --sdl-source /tmp/miyoo-sdl2
```

The packager archives `HEAD`: commit changes before packaging. Output is
`onionos/dist/Carmageddon-OnionOS-experimental-0.1.4.zip`. Packaging uses an explicit
allowlist and does not read your DATA/MUSIC. Never add these directories to Git.
`SHA256SUMS.json` covers all payload files except itself.

To verify linking, use `readelf -d` on the engine and SDL2. Neither may depend on
libEGL/libGLES/libGL. MI vendor libraries are resolved at link time through
`MIYOO_SDK_LIB_DIR` and at run time through the launcher's app-local / firmware
library paths. The app-local libraries must be ARM, not desktop x86 libraries.
