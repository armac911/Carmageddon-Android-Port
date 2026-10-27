# Validation — experimental OnionOS 0.1.2

Performed in Linux build environment, 2026-10-27. No Miyoo was connected and no
commercial game data was supplied or used.

## Completed

- ARMv7 hard-float Release compilation of the complete engine and probe with
  the Miyoo Buildroot toolchain. ELF attributes: ARMv7-A, VFPv4, NEON, hard-float.
- Engine and patched SDL dynamic dependencies inspected: no GL, GLES, EGL or
  SwiftShader dependency. SDL still requires the device's vendor MI libraries.
- Desktop build of the same Miyoo platform and rasterizer, SDL dummy video
  driver: one-second probe passed its projected triangle-centre check and
  presented 30 synthetic frames. This is a host probe, not a gameplay FPS result.
- AddressSanitizer + UndefinedBehaviorSanitizer Debug build: both raster and
  input tests passed with no reported address/undefined-behaviour errors.
  Leak detection disabled because these tests link the entire legacy engine;
  this is not a claim of leak-free full gameplay.
- Procedural raster tests cover projection, nearer/farther depth, transparent
  indexed texture pixels, row padding, sub-pixelmap bases, fully behind-camera
  rejection and a near-plane crossing. No commercial fixtures are used.
- SDL synthetic-key tests cover menu accept, simultaneous accelerator/steering,
  brake/repair, handbrake/recovery, release on focus loss and exit chord.
- Launcher/build scripts checked with `sh -n`. JSON configs parsed. `git diff
  --check` clean. Package checks cover ELF identities, allowlisted contents and
  payload SHA-256 hashes.
- `android/` has no changes relative to the base. Existing non-Miyoo CMake
  defaults remain; shared menu guards preserve the Android network-disable path.

- Existing Android Python regression scripts were also attempted. Native/render
  fixtures reported passes, but the discovery command exited with five import
  errors because the host has no `javac` (controller mapping, error export,
  game content, touch overlay and USB registration). These Java fixtures were
  not validated; none of their source files were changed.

## Not completed / not claimed

- Launch on actual Miyoo Mini Plus firmware/Onion, vendor-library compatibility,
  actual panel orientation, button events and return-to-MainUI behaviour.
- Real DATA menu, race loading, gameplay, saves, cockpit/mirror, sky, shadows,
  all materials/geometry, or long-session stability.
- Measured game RSS / peak loading memory, OOM margin or sustained device FPS.
- Sound/music or LAN multiplayer. Audio is intentionally disabled.
- A fresh Android APK build or new Android release. Existing releases were not
  replaced, and this branch has not been merged into main.

## Run host checks

Use desktop SDL2 headers/library and a native compiler (no cross toolchain file):

```sh
cmake -S . -B build-miyoo-host -DDETHRACE_MIYOO=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-miyoo-host -j4
cd build-miyoo-host
ASAN_OPTIONS=detect_leaks=0 ctest --output-on-failure
SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 ./miyoo-probe 1
```

Tests use `assert`: use Debug for host checks. ARM Release builds are for device
execution. See README for the device sequence and logs to return. A useful first
report includes Onion version, whether the triangle appears and is upright,
which button failed, whether the main menu/track appears, and latest.log after
that precise run.

## First hardware report and 0.1.1 fix

On Onion v4.3.1-1, the 0.1 diagnostic loaded SDL Mini successfully, ran 601
frames in 30,037 ms, passed its CPU triangle-centre check and exited normally
(status 0). It selected `renderer=software`, yielding no visible image. The
reported peak process RSS was 4,944 KiB. These are probe metrics only, not
gameplay performance or memory figures. No game-data load is established.

Source inspection confirms the pinned Mini software framebuffer update returns
without copying the surface. 0.1.1 explicitly selects `Miyoo Mini` by renderer
index, logs available renderers, disables batching and refuses the invisible
fallback on Mini. Native SDL dummy tests and the ARM build are rerun for this
change. A third injected-driver regression test lists software before Mini and
sets SDL_RENDER_DRIVER=software, then verifies the real platform requests the
explicit Mini renderer index and disables batching. All three sanitizer tests
pass. Visible hardware output remains to be confirmed.

## 0.1.2 real-data crash capture

A subsequent device log detects Carmageddon DATA and reaches 3D drawing. Its
later sampled windows report 7.80 and 6.14 FPS, peak process RSS 9,144 KiB, no
swap use and roughly 73,900 KiB available system RAM. The process exits with
139. A final stub warning is not sufficient to identify the faulting function.

This build adds an alternate-stack signal handler using async-signal-safe I/O
and local formatting, recording ARM register context and `/proc/self/maps`.
Release optimization is retained, with debug line tables for symbolization.
No crash-cause fix or gameplay performance improvement is claimed.

Checks for 0.1.2: native and ARM-under-qemu controlled SIGSEGV tests both
recorded registers and mappings and returned terminal status 139. The three
existing host raster/input/presenter tests passed. Optimized ARM binaries were
checked for `.debug_info` and `.debug_line`; launcher shell syntax passed.
Neither qemu testing nor a controlled signal reproduces the real game crash.
