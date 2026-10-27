# Carmageddon for OnionOS — experimental 0.1.1

This is an initial ARMv7 CPU-rendering experiment for **Miyoo Mini Plus**. It is
based on the Android port at `c4fadaca8aea901ee93d854a806a44334e2674da` (0.9.2).
It is **not yet verified to reach a race or be playable on hardware**. The ZIP
contains a real ARM game executable and a separate data-free diagnostic probe.
The Android application, APKs, release tags, and main branch are preserved.

## Install and test

1. Extract the ZIP on your computer. Copy its `App` folder into the root of the
   OnionOS SD card, merging folders. This creates two entries under **Apps**:
   **Carmageddon (experimental)** and **Carmageddon Diagnostics**. Keep the
   source archives and license files on your computer; they need not go on SD.
2. First run **Carmageddon Diagnostics**. For about 30 seconds you should see a
   moving colour pattern and a solid triangle. Press every button, including
   simultaneous A + Left/Right. Input events and memory samples go to the log.
   The triangle's centre is checked in code; failure exits with status 3.
3. Copy the `DATA` directory from your own compatible **PC Carmageddon 1**
   installation to `App/Carmageddon/game/DATA`. Keep the original directory
   structure and UPPERCASE names (`GENERAL.TXT`, `RACES`, etc.). This first
   experiment targets the original game; Splat Pack is not validated. Android
   APK assets are not a substitute for PC game data. No EXE is needed on SD.
4. Start **Carmageddon (experimental)**. Try the main menu, start the first
   race, drive briefly, and test steering while accelerating. A accepts in menus;
   B or Menu backs out. Use the default driver name, since there is no text-entry
   keyboard yet. Try external camera first; cockpit/mirror cost more and remain
   unverified. No game files means it runs a 10-second probe and exits with status 4.
5. Return via the game's Quit option, or press **Menu + Start** together. The
   launcher resumes MainUI after normal exit or a game process crash.
6. Send `App/Carmageddon/logs/latest.log` (or zip the entire `logs` folder on your
   computer), plus a photo/video and where it failed. Do not include your DATA.

Use a **copy** of your PC DATA: original game routines can write settings and
saves there. This test does not integrate Onion GameSwitcher, quick resume or
RetroArch save states. Start it from Apps. No firmware/library replacement,
overclock, swap or Android install is involved. To remove it, delete these two
App folders, after keeping any saves/logs you want.

## Controls

Mapping uses the physical key definitions in OnionUI's `keymap_sw.h`, with
current game action bindings for driving/repair/recovery. Use PC keyboard
bindings in the game DATA; joystick-only action bindings are not supported.

| Miyoo control | In a race | In menus |
| --- | --- | --- |
| D-pad Left / Right | Digital steering | Navigate |
| D-pad Up / Down | Accelerate / brake or reverse | Navigate |
| A | Accelerate | Accept / Enter |
| B | Brake or reverse | Back / Escape |
| X | Handbrake | — |
| Y | Recover car | — |
| L1 | Repair, hold | — |
| R1 | Toggle cockpit / external camera | — |
| L2 | Original gear-down action | — |
| R2 | Horn | — |
| Select | Map | — |
| Start | Enter | Accept |
| Menu | Escape / pause or menu | Back |
| Menu + Start | Exit process | Exit process |

Automatic transmission stays active. There is no new manual gear-up system,
analog steering, touch UI, haptics, remapping UI or cheats toggle in this build.

## Initial profile and limits

- Original low-resolution canvas (normally 320×200), corrected to fill a 4:3
  area on the 640×480 panel. This is intentionally low fidelity.
- Indexed CPU triangle rasterization, perspective-correct nearest textures,
  six-plane homogeneous clipping, 16-bit depth, palette/shade/blend lookups.
  SDL's Mini 2D blitter only presents the finished frame; no 3D GPU is used.
- `-lomem -nosound -nocutscenes -noreplay -yon 0.35 -simple 4 --fps=30`.
  Thirty is a requested cap, **not a measured device frame rate**.
- No OpenGL, GLES, EGL or SwiftShader runtime. Audio is disabled for this test;
  the original miniaudio dependency remains compiled, not initialized for sound.
- Environment mapping, exact lighting, custom clip planes, one-sided face
  culling and original edge rules are incomplete. Artifacts, incorrect shadows,
  depth effects or mirror behaviour are possible. This is a baseline renderer.
- LAN/network selection is disabled, as on Android. Music and sound are not
  claimed to work. No original assets, DATA, MUSIC, ROMs or installers included.

## Logs

`logs/latest.log`, `previous.log`, and `older.log` keep three launches. Both Apps
share this directory, so copy the relevant log before more launches rotate it.
Logs include build identity, kernel/CPU, memory, framebuffer/input devices,
SDL driver/renderer, physical key events, process RSS/high-water memory every
five seconds, FPS/triangle totals and process exit status. They do not copy
assets. Peak RAM and FPS with a real race are **still unknown**.

Status 139 usually means SIGSEGV; 137 means SIGKILL (possibly OOM, not proof).
Loader messages identify missing/incompatible libraries. Check this log even if
the app immediately returns to Onion. A black screen alone is not a diagnosis.

For design reasoning, builds and actual checks see [FEASIBILITY.md](FEASIBILITY.md),
[DEPENDENCIES.md](DEPENDENCIES.md), and [VALIDATION.md](VALIDATION.md).

## 0.1.1 display fix

The first hardware log showed the 0.1 probe ran for 30 seconds, passed the CPU
triangle check and exited with status 0, but selected SDL's `software` presenter.
In the pinned Mini fork, `Mini_UpdateWindowFramebuffer` returns success without
copying pixels to the display. SDL lists this invisible software route before
its working `Miyoo Mini` renderer. Version 0.1.1 selects the latter by explicit
index on Mini video and disables batching. This only uses the MI_GFX 2D blitter
to display finished frames; 3D remains CPU-rendered. A missing Mini presenter
now produces a clear error rather than silently choosing an invisible fallback.

Extract the new App folder over the existing installation, replacing files.
Keep `game/DATA` and saves. Re-run Diagnostics and return the new latest.log.
Actual visible output with this fix still requires device confirmation.
