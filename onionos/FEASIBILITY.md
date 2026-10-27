# Technical feasibility assessment

Assessment date: 2026-10-27. Base: Android-port commit
`c4fadaca8aea901ee93d854a806a44334e2674da`.

## Conclusion

An ARMv7 Linux build and a CPU rendering path are technically achievable: both
now compile. **Loading and driving a real race within the available memory and
at useful speed are unproven.** This branch is a device experiment, not a claim
that an Android APK can run on OnionOS or that the port is finished.

## Target and architecture

Target Miyoo Mini Plus: dual Cortex-A7, normally 1.2 GHz, ARMv7 hard-float Linux,
640×480 display, nominal 128 MB RAM shared with the OS. Actual memory exposed to
the process and available on this specific firmware must be measured from the
included logs. No usable hardware OpenGL/GLES 3D route is assumed. CPU settings
come from the [Mini SDL backend](https://github.com/steward-fu/sdl2) and the
[Miyoo toolchain](https://github.com/OnionUI/dev-miyoomini-toolchain). Display and
CPU specifications are also listed by [Miyoo](https://lomiyoo.com/products/miyoo-mini-plus).

The Android app is ARM64 + Java/JNI + SDL2 + GLES3/EGL. Miyoo is a separate CMake
option (`DETHRACE_MIYOO=ON`), ARMv7 executable, filesystem launcher, SDL2 Mini
presenter and indexed CPU rasterizer. Java, JNI, Android storage permissions,
launcher UI and Android graphics code are not part of this executable. Default
build selection remains the existing SDL/OpenGL/Android path.

## Rendering

`src/harness/renderers/gl/gl_renderer.c` implements this fork's actual 3D path;
its BRender compatibility layer does not supply a finished portable software
triangle backend. Merely using SDL_RENDERER_SOFTWARE would replace presentation,
not this GL-based 3D pipeline. Running GLES through SwiftShader would add CPU and
memory cost and still need all GL features, so this experiment avoids it.

The new rasterizer consumes BRender's prepared vertices/faces and material
pointers directly; no duplicate texture/model cache is allocated. It writes
indexed colour and depth into engine-owned pixelmaps, respecting colour/depth
row strides and sub-pixelmap bases. CPU effects see current data without GPU
readback. Engine depth clears remain authoritative across scene boundaries.
Homogeneous clipping occurs before perspective division; perspective texture
coordinates and transparent index zero are supported.

The implementation intentionally accepts both face windings. Missing exact
lighting, custom clip planes, environment maps and original raster edge rules
mean it is not feature-equivalent to the GL renderer. Main-menu assets and 3D
race scenes still require real-data validation. A simple procedural triangle is
not evidence that a track, cockpit, shadows or pedestrians render correctly.

## Memory

At a 320×200 canvas, one indexed plane is 64,000 bytes, one 16-bit depth plane is
128,000 bytes and the presenter's ARGB conversion buffer is 256,000 bytes. The
presenter alone allocates the last buffer plus SDL texture/backend buffers.
A single full-panel 640×480 ARGB surface is another 1,228,800 bytes; the Mini
backend creates multiple working/display surfaces. These are arithmetic sizes,
**not measured total game memory**. Track geometry, textures, pedestrians, cars,
BRender resources, SDL, executable mappings and OS/vendor allocations dominate
the remaining budget and cannot be inferred from the framebuffer sizes.

`-lomem`, disabled replay, sound and cutscenes avoid unnecessary loads. Reduced
view distance and car detail reduce CPU work but do not guarantee track data
will fit: much of the level can still be loaded. The game executable has about
2.7 MB text/data/BSS before dynamic libraries and assets; this is not RSS. No
swap or automatic overclocking is introduced. Log VmRSS/VmHWM/VmPeak plus system
MemFree/MemAvailable during menu, loading and driving. A sensible development
goal is substantial headroom below available RAM throughout a race; no fixed
safe ceiling can be asserted without the device measurement.

## Dependencies and compatibility

- CMake, ARMv7 hard-float Buildroot GCC/sysroot, SDL2 Mini fork.
- Existing vendored BRender, libsmacker and miniaudio; libm/dl/pthread/libc.
- SDL Mini needs firmware MI_GFX/MI_SYS/MI_COMMON/MI_AO/shmvar libraries and
  json-c. The test ZIP bundles only patched SDL2 and MIT json-c; vendor system
  libraries must come from the Miyoo firmware in `/customer/lib` or `/config/lib`.
- This build uses the Miyoo toolchain's glibc ABI. ELF version inspection finds
  GLIBC up to 2.27 in the game and up to 2.28 in SDL. Whether the exact installed
  firmware satisfies these requirements is a device-test gate. No private
  glibc/loader is installed over the OS.
- The SDL fork has an unconditional Mini EGL adapter even with GL disabled;
  `prepare-sdl.py` removes it and the EGL/GLES link dependencies, and removes the forced renderer flags. The Mini generic software
  framebuffer updater is a no-op, so 0.1.1 explicitly selects its MI_GFX presenter.
- The initial build skips audio initialization via `-nosound`. Enabling SDL
  audio alone would not adapt miniaudio's output to the MI_AO backend.

## Controls and device-test gates

Button definitions follow
[OnionUI keymap_sw.h](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_sw.h).
SDL keyboard events are translated to the engine's DirectInput state/action
slots. This avoids requiring a joystick or analog axes. Simultaneous steering
and acceleration, release on lost focus, menu accept/back and Menu+Start quit
have synthetic-event tests; actual kernel/SDL events still need the device.

Progress gates: (1) loader + SDL probe, (2) every physical key in logs, (3) PC DATA
menu load, (4) first track loads without OOM, (5) drive and inspect the picture,
(6) measure sustained FPS and RSS, (7) return to Onion. Audio, renderer fidelity,
performance tuning and broader data variants follow only after these gates.
