# Milestone 0.2 — complete core driving controller mapping

Install over the current Carmageddon Android Port / Carma Android Next without
uninstalling. This milestone keeps the same application ID and development key.
The version shown by Android and the launcher is exactly **0.2**.

Controller mapping:
- Left stick and D-pad left/right: steering.
- RT or X: accelerate.
- LT: brake / reverse.
- LB: handbrake.
- Y: recover / respawn.
- RB: repair (hold).
- Start: pause / menu.
- Menus: stick or D-pad navigation, A confirm, B back.

Driving actions use the loaded game keymap and merge with touch input. Polling
clears released, disconnected and unfocused input. SDL standardized button names
are used. Camera view, map and manual gear controls still lack dedicated race
controller buttons. Menu confirmation/back remain available on A/B.

The owner confirms shadows are fixed on Flip 2 in 0.1.19. Its diagnostic capture
at 16:9 High/Far/Sharp averages 49.4 FPS after startup/interrupted-frame exclusion.
Average readback/synchronization time is 8.74 ms, versus 0.70 ms CPU updates.
Later scenes have more draw calls and readbacks. No thermal warning was reported.
This is not enough evidence for a safe quick renderer optimization; the milestone
focuses on input and retains the tested shadow fixes. Future work can investigate
GPU/CPU synchronization using controlled scene comparisons.

Diagnostic metadata now writes effective default settings (e.g. cap 60 and
analog steering) rather than the ambiguous string "default". Recording remains
opt-in. Please test each assigned button, combined steering/acceleration and
repair, then pause/resume and disconnect/reconnect if practical.

README includes the AI disclosure, upstream credits, feature list, controls,
build/import instructions, licensing and validation limits for future publishing.

## Earlier release notes

# Version 0.1.19 — cross-GPU shadow compatibility test

Install over 0.1.18 without uninstalling. Package, key and game-data location
remain compatible. The owner confirms S24 FE looks good in 0.1.18; a Flip 2
recording shows shadow flicker and colored streaks across the road.

This version makes three renderer corrections:

- GLES geometry, clipping and index calculations request high precision rather
  than inheriting mediump defaults. Lower precision can lose small camera or
  surface offsets on hardware implementing mediump with half precision.
- Android's depth attachment uses 24-bit depth for better separation of closely
  spaced road/shadow geometry.
- Indexed blending samples its GPU-copied snapshot at matching bottom-left
  coordinates. The old vertical flip fetched unrelated rows, and height-y
  accessed outside the snapshot at the first row. A varying-background GPU
  regression reproduced 56 incorrect pixels out of 64 before the correction.

The Flip 2 result is not yet confirmed. First repeat the recorded area with
Sharp textures, then Smooth. Check the red streaks, car shadows, smoke, glass
and oil where available. A short S24 FE check can confirm that its previously
correct behavior is retained. Enable/export diagnostics only if useful for
further analysis; recording remains opt-in.

## Earlier release notes

# Version 0.1.18 — bilinear 3D textures and launcher website link

Install over 0.1.17 without uninstalling. Package and signing key are unchanged.
Choose **Graphics → 3D texture filtering → Smooth — bilinear textures**, then
press Play. An existing Smooth choice from 0.1.17 carries over automatically.

Filtering now runs in the 3D texture shader. Four shaded texel colors are
interpolated and mapped back into the game palette through a 32×32×32 lookup
cube. This keeps the original indexed render target, CPU effects, blend tables,
shadow pipeline and nearest-texel cutout coverage. Transparent neighbors are
excluded and weights renormalized to avoid transparent-color fringes. Flat
regions retain their exact index. The table is regenerated on palette changes.

The old final-image blur is disabled; HUD, menus and on-screen touch controls
stay sharp. This is palette-limited bilinear filtering, not a full true-color
renderer or MSAA. Device appearance and performance still need confirmation.

The entire launcher “Android port by” credit/logo block opens
https://androgaming.com through the browser. No branding was added in-game.

Please compare Sharp and Smooth on the same nearby wall/road/car texture.
Look for smoother texture blocks while checking that text remains crisp.
Check cutout textures, smoke/transparency, shadows and scene transitions too.

## Earlier release notes

# Version 0.1.17 — filtering, app identity and opt-in diagnostics

Install over Carma Android Next / Carmageddon Android Port 0.1.15–0.1.16.
The application ID and signing key are unchanged, so keep the app installed.
Existing game data, saves and graphics preferences remain available.

The display name is now **Carmageddon Android Port**, with the maintainer's
supplied replacement icon. AndroGaming branding stays in the launcher.

Graphics now offers **Image filtering → Sharp / Smooth (bilinear)**. Sharp is
the default. Smooth blends decoded palette colors during final presentation,
including menus/HUD; it does not replace textures or add MSAA/FXAA. The 3D
source grid is retained in 3D regions. It may soften text; compare on the phone.

Recording and the performance overlay switch off once on this update. They
remain opt-in afterward; user choices are remembered and previous exported
or stored recordings are not removed. Diagnostics can be re-enabled manually.
New captures include the selected image-filtering mode in metadata.

README.md is now a public-repository draft with an explicit AI-use disclosure,
upstream credits, feature inventory, data/import instructions, build commands,
validation limits, signing history and artwork/license notes. The inherited
README is preserved under docs. No repository has been published yet.

Device check: verify new name/icon, confirm recording and overlay start off,
compare Sharp and Smooth in the same scene and menus, then re-enable capture
only if needed. 0.1.17 has passed synthetic GPU checks and build validation;
phone appearance and filtering cost have not yet been measured.

## Earlier release notes

# Version 0.1.16 — AndroGaming launcher

Install this APK over **Carma Android Next 0.1.15**. It uses the same package
and recovered signing key. Existing game data, saves and preferences remain.
Do not uninstall first. This still does not update the older Carma Android Dev.

The landscape launcher now has a dark charcoal/red design, a dedicated Home
page with Play and a settings summary, and separate Graphics, Controls and
Diagnostics pages. AndroGaming's supplied logo and “Android port by” credit
appear only in the launcher sidebar. Tapping the logo opens androgaming.com.
No branding or splash screen was added to the game. Engine code is unchanged.

Graphics keeps aspect, resolution, distance and all four FPS options. Controls
keeps steering, haptics and the existing touch layout editor. Diagnostics keeps
the optional in-game monitor, profiling and ZIP export. Preferences save as
soon as an option is chosen and apply to the next Play session. Home hides the
import button once game data is ready. No game content is bundled.

Please check the launcher on your device: logo/readability, page navigation,
settings persistence after closing/reopening, launching your existing data,
touch editor access and diagnostic export. The screen uses scrollable content
and navigation to accommodate compact landscape displays and larger fonts.

## Previous release notes

# Version 0.1.15 — selectable frame-rate cap

The launcher offers **60 FPS (default), 120 FPS, 144 FPS and Uncapped**.
Choose before pressing Play. The shared presenter applies the cap in both
menus and races, including repeated menu redraws. Slow frames do not trigger
catch-up bursts. Uncapped removes the software wait; Android may still limit
presentation. Selecting a higher cap does not force a higher display refresh.

The old limiter skipped waits of 5 ms or less and missed some menu redraws.
The new limiter uses a high-resolution monotonic clock and sleeps for the
remaining interval, rechecking the clock before presenting. Android requests
swap interval zero to avoid stacking vsync waits on the software cap. Wait
time remains in profiling; diagnostics also record the chosen cap.

## Installation and signing

This APK installs separately as **Carma Android Next**
(`com.androgaming.carma.next`). The temporary environment reset and the old
signing key was not preserved. This APK therefore cannot update 0.1.14 under
its original identity. Keep the old app installed to retain its saves and data.
Import your own game data into the new app. Game content is not bundled.

A separate private signing-recovery archive preserves the new debug key for
future updates of Carma Android Next. Do not publish it. Restore its key to
~/.android/debug.keystore before future debug builds. This is development
signing, not a production release key.

## Device check

Try 60 FPS in menus and a race, then export diagnostics. Repeat with a higher
cap to compare. Caps are ceilings, not performance guarantees. Slow scenes
remain limited by rendering time. No physical-device test of 0.1.15 was run.

## Earlier release notes (historical installation instructions)

# Carmageddon PC to Android: development starter

Prepared for Dejan / AndroGaming, 18 October 2026.

## Version 0.1.14 — merge shadow and main-scene readback

The owner confirms 0.1.13 shadows, transparency and smoke look correct. The
capture shows the starting-grid readbacks falling from ~6.5 to 2 per frame,
and measured frame time from 32.2 to 19.0 ms in seconds 5–10 at High/Far.
Those runs have different scene workloads; the comparison is not a benchmark.

This revision keeps the finished shadow layer in a separate GPU texture, clears
only the active colour layer, and combines them during the next required CPU
readback. A nonzero scene palette pixel wins; otherwise the shadow pixel is
used. Sampling remains identical at all three resolution levels. Depth and
blend snapshots retain the 0.1.13 behavior; blends never sample the deferred
shadow layer. The pending layer is consumed once, even if no main geometry was
drawn, and reset on frame clear. Oil-intersection fallbacks remain intact.

This removes the separate shadow readback in the ordinary path without rewriting
CPU fog/smoke/HUD effects. Other explicit CPU boundaries remain. The extra GPU
texture costs one byte per supersampled canvas pixel (~3.6 MiB at High 16:9).
Synthetic shadow-plus-main readbacks fall from two to one. Actual FPS improvement
and runtime transfer counts need a device test; no distance/quality reduction.

Install over 0.1.13 without uninstalling. Test **16:9 + High + Far (2×)**, stay
stationary for 20 seconds then drive 30 seconds, quit and export diagnostics.
Check shadows, transparent objects, oil, smoke, menus and scene transitions.
Profiling remains enabled unless you previously disabled it in the launcher.

## Version 0.1.13 — fewer synchronous readbacks

Profiling on the owner's S24 FE points to readback/CPU copying, especially at the
starting grid. A deeper trace also finds separate scene-end readbacks for each
nearby car's shadow, not only the blending snapshot path.

- Batch GPU-only shadow passes sharing the same buffers, retaining camera,
  clip-plane, shade-table and depth changes. Read back once after the batch.
- Oil-intersecting shadows keep separate boundaries as a conservative fallback.
- Prepare the existing indexed-blend snapshot on GPU without reading/clearing
  the framebuffer. Keep accumulated colour until an actual CPU consumer flushes.
- Preserve explicit synchronization before CPU depth effects and smoke, and at
  normal scene end. The palette shader and its existing blend conventions stay
  unchanged; this is not a transparency algorithm rewrite.
- Empty batches are safe before the first scene; snapshot bookkeeping initializes
  explicitly for each renderer startup.

Install over 0.1.12 without uninstalling. Keep **16:9 + Far (2×) + High** for the
first comparison. Record 20 seconds stationary then 30 seconds driving, quit,
and export diagnostics again. Profiling stays enabled. Check car shadows, oil,
transparent surfaces, smoke and menus. Draw distance, resolution and assets are
not reduced. Device FPS gains remain unmeasured until the owner's new test.

Real headless GLES checks use production flush/batch/snapshot functions and the
production palette fragment shader. Six synthetic shadow flushes become one;
blend preparation plus final readback becomes one instead of two. Palette output
matches the eager reference at 1×/2×/3× with overlapping draws, depth tests,
non-writing blended depth, transparent gaps, CPU pixel edits, and empty batches.
These counts describe the synthetic test, not every real race frame.

## Version 0.1.12 — exportable race profiling

Install over 0.1.11; keep app data. Recording is enabled by default in this
build and can be disabled with **Record performance diagnostics** in the launcher.

1. Set 16:9, Far (2×), High, FPS + battery temperature.
2. Start the same first race/car. Stay stationary for 20 seconds, then drive
   for 30 seconds. Avoid menus during this interval.
3. Exit through the game's quit option. Let the phone cool, reopen the launcher,
   change only High to Performance, and repeat the same race and route.
4. Exit the game and select **Export performance diagnostics ZIP** in the
   launcher. Choose Downloads in Android's file picker and send the ZIP.

Use an unplugged phone without screen recording. Start both runs at similar
battery temperatures. No ADB is needed. The ZIP keeps up to the latest 12 launch
sessions, including both tests. Settings are read at launch; fully exit before
changing them. No game data, save names, serial numbers or account data is exported.

Each session contains metadata.json, sensors.csv and frames.csv (created when
a race starts). Native recording stops after 120 seconds of non-interrupted race
frame time per launch; sensor samples are taken every two seconds while the game
activity is resumed, up to 300 samples. Older sessions are retained locally;
only the latest 12 are exported. Files are buffered and flushed every second,
on interrupted frames and on normal process exit. Force-stopping can lose the
last second. Menus are not recorded as race frames; a race frame interrupted by
pause/menu is flagged excluded=1 and must be excluded from comparisons.

The frame CSV captures CPU wall times for game update, full rendering, readback,
CPU pixel copy/clear, model submission, final presentation, buffer swap and FPS
limiter. Rendering contains nested phases: do not add all timing columns. Swap
may include vsync or GPU waits; readback includes queued GPU work, downsampling
and transfer, so it is not a pure GPU execution measurement. No glFinish or GPU
timer queries were added. Timings exclude CSV logging after the frame; logging
has some overhead. Model draw-call/triangle counters exclude fullscreen quads.
Counters also include models considered/culled, material applications, model
uploads and readback bytes. Driver/vendor/version strings are in the CSV header.
The session_start_elapsed_ms header aligns frame timestamps with sensor timestamps.

Battery °C is not CPU/GPU temperature. Android thermal status, charging and
power-saving state accompany it; unavailable readings use blank/-1 values.
Graphics settings and draw distance are unchanged. This is a diagnostic build,
not another performance optimization. Phone capture/export still needs testing.

## Version 0.1.11 — reduce off-camera work at Far distance

Source investigation found BrOnScreenCheck is a stub in this older renderer
fork. Track selection includes a rectangular region around the view, but the
stub accepts every model in that region, including those outside the view.

- Add conservative model bounding-box rejection in the Android GLES renderer,
  before matrix uploads, material setup and draw calls. Bounds are built from
  the actual uploaded vertices and rebuilt with deformed meshes. Plane tests
  use the same view/projection/model matrices as the shader. Edge-touching and
  invalid/uncertain bounds remain visible; no distance or small-object cutoff.
- Skip normal-matrix inverse/transpose work for ordinary materials. Keep the
  existing calculations for environment-mapped materials that actually use it.
- Retain Far (2×), scenery coverage, assets and all chosen graphics settings.

Install over 0.1.10. Compare the same race start at **Far (2×)** with the same
resolution and aspect ratio. Turn the camera and drive around to check for
missing/popping scenery, pedestrians, cars and reflections. Report FPS at the
start and after driving away from the grid. Runtime gains remain unmeasured
until tested on a physical device. See PERFORMANCE-NOTES.md for findings.

## Version 0.1.10 — first performance pass

The owner reports 15–18 FPS at race start in 0.1.9; the exact graphics settings
and device thermal state were not supplied. This release targets renderer work
identified in source review. No on-device speedup has been measured yet.

- Downsample the GPU palette framebuffer before synchronous CPU readback.
  Standard now transfers one quarter as many colour pixels; High transfers one
  ninth. The shader selects exactly the same samples as the previous CPU path.
  This reduces transfer volume, not necessarily total frame time by that ratio.
- Allocate recurring presentation/snapshot/blend texture storage once and update
  it in place, instead of reallocating it during presentation and blending.
- Fill Android's existing placeholder CPU depth buffer directly, eliminating a
  redundant supersampled clear/downsample. This does not add depth effects.
- Add **3D resolution: Performance** (1× game canvas) alongside Standard (2×)
  and High (3×). Performance reduces 3D pixel work; the high-resolution game
  canvas, HUD, menus, 4:3/16:9 modes and original assets are retained.

Install over 0.1.9. First compare the same race start with the same graphics
settings to isolate the renderer changes. Then try **Performance + Original
draw distance** for the fastest available settings. Report FPS, resolution,
draw distance and battery temperature for each run. Existing saved graphics
settings remain selected; this update does not silently lower quality.

A headless Mesa GLES test runs the production downsample shader and readback
function. It confirms exact palette samples (including zero), sub-pixmap source
coordinates and restoration of GL state needed by mid-scene blending. Real
Exynos rendering, HUD, transparency, mirror and frame-rate tests remain pending.

## Version 0.1.9 — steering modes and performance overlay

The launcher now offers **Steering (touch and left stick): Analog / Digital**.
Analog touch retains the proportional slider; gamepad left-stick steering now
uses its full proportional range beyond a deadzone. Digital touch displays
left/right arrows and digital mode uses the original left/right key behavior.
D-pad input remains full-direction input. Gas/brake mappings are unchanged.
Analog is the default. The editor previews the chosen steering style, retaining
saved size and position. Configure settings before Play.

**Performance overlay: Off / FPS / Battery temperature / FPS + battery temperature**.
The small top-right display stays visible when gamepad input hides the touch
controls and does not consume touches. FPS measures SDL frame presentations
in roughly one-second windows, not the configured cap. Sampling restarts after
suspension; zero can appear before the first sample or if frames stop.
Temperature is explicitly labeled Battery and displayed in °C from Android's
battery status; it is not CPU/GPU temperature. Devices without a reported battery
sensor show N/A. Readings reflect the latest value supplied by Android.
Overlay polling stops while the activity is paused and is disabled by default.
No extra permissions, root access or external monitoring apps are required.

Install over 0.1.8 without uninstalling or clearing data. Existing imports,
saves, aspect ratio, graphics quality and control layouts are retained.
Build and host checks pass; device steering feel, overlay placement and actual
battery sensor reporting still need testing on the S24 FE and gamepad devices.

## Version 0.1.8 — automatic input switching and graphics options

Gamepad button presses and meaningful stick/trigger movement hide the touch
controls. Touching the screen restores them immediately during a race. Merely
connecting a controller, releasing an axis, or small stick drift does not hide
them. Switching clears held touch inputs. Controller mappings are unchanged.

Before Play, choose the launcher graphics settings:

- Draw distance: Original, Extended (1.5×), or Far (2×). Extended/Far increase
  the camera far plane and remove the old reduced scenery coverage factor.
  They retain the saved PC distance setting as the base and leave AI settings alone.
- 3D resolution: Standard (2×) or High (3×). High sharpens geometry edges with
  the original assets. HUD/menu resolution is unchanged.

Defaults preserve the previous graphics settings. Try Extended + Standard
first on S24 FE, then Far and/or High if frame rate remains acceptable. High
renders 2.25 times as many 3D pixels as Standard. Existing textures, fog and
level geometry still limit what can be seen. No replacement assets are required.
Install over 0.1.7; do not uninstall or clear app data. Device performance and
controller/touch transitions still require physical-device testing.

## Version 0.1.7 — readable text entry and configurable icon controls

The owner's 0.1.6 recording shows usable driving controls but the text editor
collapses vertically above the Samsung keyboard. The editor now uses a light
Material dialog, an explicit 56 dp field, 20 sp dark text, no title/message
block, and top positioning with IME resize. Done/Cancel keep their behavior.
The keyboard layout itself remains controlled by Android/the keyboard app.

In the launcher, choose **Customize touch controls** before Play:

- Set overall size from 70% to 140%.
- Drag individual buttons and the steering pad in a scaled game-aspect preview.
- Use Standard or Mirrored presets, or Reset to the original 100% layout.
- Tap an icon in the editor to see its action name. Save commits the layout;
  Cancel leaves the saved layout unchanged. Overlapping placements are rejected.
- Icons replace gameplay labels: accelerator pedal, brake/reverse pedal,
  parking brake, wrench (hold repair), recovery arrow (hold 0.6 seconds),
  camera, folded map and pause. Steering keeps its slider/knob.

Positions and size persist across launches. Configure controls while the game
is closed, then launch it. No game data or saves are replaced by the update.
Build and host layout tests pass; keyboard visibility and editor behavior still
need physical-device confirmation.

## Version 0.1.6 — keyboard, touch controls and haptics

The owner reports 0.1.5 looks good and save/load works on the Galaxy S24 FE.
Install 0.1.6 over the existing APK without uninstalling or clearing app data.

- Tap a name/save-name field to open the Android text editor and keyboard.
  Done submits it through the original game interface; Cancel preserves the
  existing text. The editor supports printable ASCII and the field's own
  length limit, including selection, replacement, deletion and paste.
- Bottom left: drag the steering pad for proportional left/right input.
- Bottom right: hold GAS or BRAKE/REVERSE; HANDBRAKE sits above the brake.
- Above steering: hold REPAIR; hold RECOVER for 0.6 seconds to request recovery.
  The game's normal costs, restrictions and recovery timing still apply.
- Top right: CAMERA toggles cockpit/external view, MAP toggles the map,
  PAUSE opens the game menu. Buttons highlight while held.
- Multiple fingers can combine steering, pedals and actions. Releasing a
  finger only releases its action. Controls reset on focus loss or pause.
- Haptic feedback gives brief press cues, recovery-hold acknowledgement and
  rate-limited collision cues. It respects Android haptic settings and can
  be disabled in the launcher. No continuous steering/engine vibration.

Touch hit testing and drawing now share the same geometry. Native actions use
loaded game key bindings. 4:3 and 16:9 remain the only display choices.
Physical-device checks still needed: Samsung keyboard entry and Done/Cancel,
steering feel, simultaneous controls, repair/recovery, and haptic strength.

## Version 0.1.5 — 4:3 and native 16:9

Install over 0.1.4 without uninstalling to preserve imported data.
The launcher has exactly two display options: **4:3** and **16:9**.
The old full-device stretch setting is removed. Existing installs default to
4:3 until 16:9 is selected. Select the ratio before launching the engine.

16:9 uses an exact 864x486 canvas, rendered at 1728x972 for the 3D pass.
The camera uses the wider canvas aspect with its vertical FOV retained.
The viewport fits this ratio with bars on wider phones or taller tablets.
The original 640x480 HUD is centered without stretching; 320x200 menus are
doubled and centered. Menu pointer coordinates use the same offsets.
Cockpit artwork remains original-size; scenery extends beyond its side edges.
No new controls or game features are included in this revision.

Build, menu blit sanitizers, cursor regression, and viewport/pointer tests pass.
Physical-device verification is still required: compare a stationary race
camera in both modes, test menu taps and pause/resume, inspect HUD/cockpit and
map screens. The wider rendering target increases GPU/readback work.

## Version 0.1.4

The owner confirms the cursor correction works. Selecting Network Game crashes:
this branch's retail multiplayer startup is NOT_IMPLEMENTED.

- On Android, Network Game is disabled for menu navigation and its touch/mouse
  area has no choice or mode, so tapping it cannot activate a previous item.
- The existing label is muted with a checkerboard mask when the menu opens.
- Multiplayer startup returns unavailable on Android as a defensive guard.
- Single-player controls, mouse correction and fill-screen setting are unchanged.

Build and packaging validation passed. Disabled menu behavior still needs a
physical-device check with taps and gamepad/keyboard navigation.

Next priorities: complete on-screen repair/recovery/handbrake/camera actions;
proportional gamepad steering/pedals and remapping; save/load and lifecycle
verification; then expanded-FOV widescreen and graphics quality settings.

## Version 0.1.3

The 0.1.2 S24 FE recording confirms display fill works, but the cursor still
appears garbled. This revision corrects the masked sprite drawing routine:
it must use literal source coordinates, rather than adding a PIX sprite's
origin metadata to the source pixel address. The added offset can read beyond
the image when drawing the full cursor. Destination coordinates also return
to the original helper's raw-coordinate convention.

The new test extracts the actual engine function and exercises it with a
synthetic sprite carrying nonzero origin metadata. Before the correction,
AddressSanitizer reports an out-of-bounds read. After the correction, tests
for sprite origins, transparency, padded rows, clipping and palette conversion
pass with address/undefined-behavior sanitizers. The owner's cursor assets
have not been supplied, so on-device confirmation is still required.

Fullscreen fill and pointer coordinate mapping from 0.1.2 are retained. Install
as an update without uninstalling or clearing storage. No data reimport needed.

## Version 0.1.2

The owner's 0.1.1 recording on S24 FE shows menus and an active race rendering
without the earlier narrow crop. Side bars are the original 4:3 layout.

- Launcher adds **Fill screen (stretch 4:3 image)**, on by default. Uncheck it
  before starting a new game session to preserve the original aspect ratio.
  Fill changes presentation only; it does not add horizontal field of view.
- Input now reverses the exact high-res menu blit: 320x200 doubled at (0,40)
  inside 640x480. The previous height-ratio mapping missed that 40px offset.
- One shared layout handles pointer mapping and viewport fit/fill, including
  different logical-window and physical-drawable sizes. Clicks in unused
  side bars or menu margins are ignored rather than clamped onto menu items.
- Original high-res mode and 2x scene rendering remain enabled. Experimental
  widened engine buffers remain disabled pending a proper widescreen solution.

Install this APK as an update. Do not uninstall or clear storage. This revision
has the same debug certificate and a higher version code. Its device behavior
still needs confirmation. No game data changes are required.

## Current milestone

This is a development fork of the existing Dethrace Android port, not a finished
new engine. Upstream: https://github.com/mgrz18/dethrace-android
Baseline commit: c64e8c893acc455a4e7b509359de43ed23199e4b (android-port).
Original engine: https://github.com/dethrace-labs/dethrace

The baseline contains its own older BRender integration and GLES renderer.
Do not replace it wholesale with current Dethrace main: the APIs differ.
Forward-porting engine fixes is a separate planned milestone.

Changes in this starter:

- Package ID `com.androgaming.carma`; Java namespace stays `com.dethrace.android`.
- arm64-v8a, Android 7+ minimum, compile/target API 35, OpenGL ES 3 required.
- NDK r28c, AGP 8.6.1, Gradle 8.7, JDK 17, CMake 3.22.1, SDL 2.30.10.
- Default arguments `-hires --fps=60`: original 640x480 graphics mode,
  original 4:3 dimensions and a 2x-per-axis 3D framebuffer (960 high).
  These are original assets at a higher rendering resolution, not HD textures.
- Launcher with Android document-picker ZIP import before the engine starts.
- No game data in the APK; no all-files or legacy storage permissions.
- Initial SDL standardized gamepad mapping, hotplug detection, deadzone,
  menu inputs, independent keyboard/touch/pad state merging.
- Touch input restricted to racing; releases do not cancel another finger
  holding the same action. Input cleared when the SDL window loses focus.
- Removed a display-dependent swap-interval-2 cap; use vsync plus 60fps limiter.

Read VALIDATION.md for what was actually verified. Device behavior is unverified
until tested with the owner's files. The older renderer/widescreen code has not
been certified by this work.

## Build on Windows

1. Install Android Studio and JDK 17. Open the `android` folder as a project.
2. In SDK Manager, install Android SDK Platform 35, Build Tools 35.0.0,
   NDK 28.2.13676358, CMake 3.22.1 and Platform Tools. Use Show Package Details.
3. In Git Bash, run `./android/scripts/setup-sdl.sh` from the source root.
   Alternatively download SDL2 2.30.10 source and extract its contents into
   `android/app/jni/SDL` so that its CMakeLists.txt is directly inside SDL/.
4. Create a root `VERSION` text file containing `androgaming-0.1.2-dev`.
   Android Studio creates `android/local.properties` for the SDK path.
5. From PowerShell in `android`, run `./gradlew.bat assembleDebug`.

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.
Use the same debug keystore for subsequent installs, or Android will require
uninstalling the previous build, which deletes imported data and saves.
Keep release signing keys outside source control; release signing is not set up.

## Build on Linux/macOS

Install JDK 17, curl, tar, unzip, Android SDK command-line tools. Set ANDROID_HOME.
Run `./android/scripts/install-sdk-packages.sh`, then
`./android/scripts/build-dev.sh` from the project root.
The Gradle wrapper downloads the pinned Gradle version automatically.
The SDK and downloaded toolchains are not included in this source archive.
Existing GitHub Actions workflow is updated for the same package versions.
No workflow has been submitted or run on your account by this setup.

## Prepare your own original PC game data

First test Carmageddon 1, unmodified, English version if available.
Keep Splat Pack separate for now. Do not mix its DATA into the original.

Make a ZIP with `DATA/` directly at the root. Optional `MUSIC/` goes alongside it.
Do not wrap these in a CARMA/ or Carmageddon/ directory.
Copy the entire DATA directory, not only the files checked by the launcher.
The importer normalizes DATA paths to uppercase for DOS path compatibility.

For GOG Max Pack, inspect `CARMA/DATA` and `__support/app/CARMA/DATA`:
merge the support files (especially KEYMAP_0.TXT and other KEYMAP files)
into a *copy* of the main DATA before zipping. Keep the original install intact.
Other releases may already contain the keyboard files in DATA.
Preserve optional music filenames such as `MUSIC/Track02.ogg`.

The initial launcher requires GENERAL.TXT, KEYMAP_0.TXT, CARS/ and
RACES/CITYA1.TXT; this is a preliminary completeness check, not proof that
every asset is valid. Imports stage into a temporary directory and are only
promoted when validation succeeds. Existing valid imports are not overwritten.
This milestone does not have an in-app replace/reset feature.

Install APK, open it, select Import game data ZIP, then Play.
Files go to the app's external-files `game/` directory. Uninstalling removes
that data. Back up saves before uninstalling. No copyrighted files are bundled.

## Initial controls

| Input | Race | Menus |
| --- | --- | --- |
| Touch | Left/right, gas, brake/reverse, pause | Tap menu items |
| Left stick or D-pad | Steer; up/down also act as gas/brake | Navigate |
| Right trigger / A | Gas | A confirms |
| Left trigger / B | Brake/reverse | B goes back |
| Start | Escape/menu | Escape/menu |

The current build supports analog/digital steering; triggers remain digital.
Controller remapping UI and complete controller action coverage (repair,
recover, handbrake, camera, replay) remain work items. Touch layout editing and automatic
overlay hiding are implemented in the versions documented above. Modified PC keymaps can break
the initial fixed mapping. SDL supports mapped Xbox-style and other controllers;
physical controller compatibility must be checked on each Android device.
XInput is a Windows API: Android uses Android input/HID through SDL, rather
than running Microsoft's XInput runtime. Android TV launcher support is not
implemented, although touchscreen hardware is optional in the manifest.

## First device test

Start with S24 FE and Retroid Pocket Flip 2 if available, then Android TV.
Confirm import, startup, menus, one full race, audio, saving/reloading, pause,
background/resume, 60/120Hz displays, pad disconnect/reconnect, mixed touch and
pad input, notch edges, and high-res HUD/camera correctness.
Record device/Android/GPU/controller model, game edition, symptoms and logs.

Install: `adb install -r app-debug.apk`
Launch: `adb shell am start -n com.androgaming.carma/com.dethrace.android.LauncherActivity`
Capture logs: `adb logcat -v threadtime > carma-device-log.txt`
Stop log capture with Ctrl+C after reproducing the problem.

## Development order

1. Reproduce an on-device boot/race with owned original PC data; fix crashes.
2. Complete touch and gamepad actions, analog input and configurable mappings.
3. Validate high-res/widescreen, add quality settings and performance measurements.
4. Forward-port relevant current Dethrace fixes with regression testing.
5. Device matrix, 16KB runtime testing, signed releases and source publication.

Retain upstream credits and all license files. LICENSE contains GPLv3 while
upstream README also has contradictory public-domain/noncommercial language.
Clarify those upstream terms before public distribution; this starter preserves
them without making a new licensing claim. Keep game assets and signing keys
out of the repository and release archives.


Revised 0.2 (Android build code 22): Select map, R3 cockpit/external camera,
right-stick up/down forward-gear overrides, launcher controller remapping and
ARM64 NEON masked readback merge. Automatic gearbox remains enabled. Install
over the earlier 0.2 Next/Port build; signing identity is retained.
