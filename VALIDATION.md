# Validation — 1.0

- Android ARM64 assembleDebug succeeds with JDK 17, Gradle 8.7, AGP 8.6.1,
  SDK/build tools 35, NDK 28.2 and CMake 3.22.1.
- Android metadata: com.androgaming.carma.next, versionCode 23, versionName 1.0,
  display name Carmageddon Android Port.
- Native engine version is androgaming-1.0. CMake now watches the VERSION file
  so future incremental builds refresh the native version header.
- APK v2 signing, 16 KB zip alignment and ELF PT_LOAD >=16 KB checks pass.
- Signing certificate SHA-256 matches the previous Next/Port builds:
  ad560f0512c73af1368b3f07b4869e1587557b25feba254e2d7524fe6e179d9a.
- No game DATA/ or MUSIC/ assets are bundled. Source excludes private keys,
  local SDK paths, SDL downloads and generated build output.

## Input tests

- test/android/test_controller_mapping.py passes using the production SDL
  polling function and real SDL header constants with simulated hardware.
  Covers RT/X throttle, LT brake/reverse, LB handbrake, RB repair, Y recovery,
  Select map, R3 camera, gear-axis hysteresis, custom bindings, disabled inputs,
  simultaneous actions, left-stick/D-pad steering, menu navigation, Start pause,
  focus loss, disconnect/reconnect and two-controller merging.
- test/android/test_touch_bridge.py passes, including combined controller and
  touch actions, custom loaded keyboard bindings, steering priority and reset.
- The maintainer confirms controller defaults and launcher remapping work in the
  revised 0.2 build promoted to 1.0. SDL standardized names are used. Right-stick
  gear changes are forward-gear overrides; the original automatic gearbox stays
  active. Select map and R3 camera use loaded game bindings.
- test/android/test_gear_override.py passes gear bounds and blocked-state guards.
- test/android/test_indexed_copy.py compares the masked row merge against the
  previous algorithm across transparent/opaque data, widths, strides and offsets.
- test/android/test_gpu_batch.py passes production flush/batch/blend output and
  state restoration at 1x/2x/3x with the optimized row merge.

## Performance and device status

The owner confirms the 0.1.19 shadow fix on Flip 2. The submitted Adreno 650
capture at 16:9/High/Far/Sharp yields 49.44 FPS after excluding startup and
interrupted frames, with 8.74 ms mean readback/synchronization and 0.70 ms
CPU update. Later scenes have more draws/readbacks; no thermal warning is
reported. See PERFORMANCE-NOTES.md for details and limits. No rendering speedup
is claimed for 0.2. The revised build uses an ARM64 NEON masked readback row
merge; readback synchronization, shaders and the confirmed shadow fix are retained.


## Release qualification: maintainer device report

On 18 October 2026, the maintainer confirmed installation, data import,
saving/loading, default controls and remapping. Multiple races were completed
on Galaxy S24 FE and Retroid Pocket Flip 2; car upgrades and stage 2 progression
were also tested. The report covers the two-device pass collectively and does
not assign each individual check to a specific device. No additional devices,
all-game completion, multiplayer or fully manual transmission are claimed.

Version 1.0 changes release metadata and documentation only relative to that
revised 0.2 code. Its APK receives a fresh local build/signature/alignment check;
that exact renamed APK has not yet been separately replayed on the devices.
The manual GitHub Actions workflow is supplied but has not been run on GitHub.
