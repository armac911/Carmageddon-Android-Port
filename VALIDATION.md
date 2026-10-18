# Validation — 0.8 investigation build

- Android ARM64 assembleDebug succeeds with JDK 17, Gradle 8.7, AGP 8.6.1,
  SDK/build tools 35, NDK 28.2 and CMake 3.22.1.
- Android metadata: com.androgaming.carma.next, versionCode 24, versionName 0.8,
  display name Carmageddon Android Port.
- Native engine version is androgaming-0.8. CMake now watches the VERSION file
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
  revised 0.2 driving/input base. SDL standardized names are used. Right-stick
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

Version 0.8 adds error capture and changes inherited crash reporting on top of
that base. A later unexpected-exit report holds the 1.0 milestone. The original
crash cause is unresolved. No physical-device test of 0.8 is claimed.
The manual GitHub Actions workflow is supplied but has not been run on GitHub.


## Error-capture regression checks

- test_error_capture.py: production native logger, disabled capture, 2 MiB log
  rotation, terminal signal status, original context/previous-handler chaining,
  restoration/clean stop. Linux host test; not an Android debuggerd test.
- test_error_export.py: production Java handler/storage, delegation while off or
  on write failure, bounded exception stacks/logs, retention, allowlisted ZIP,
  raw trace preservation and path containment.
- Manifest uses CarmaApplication for early Java-handler setup. No READ_LOGS,
  network, root or new broad-storage permission is requested.
- Android logcat availability, native tombstone recovery and export after an
  actual phone crash remain unverified. Error recording defaults to false and
  is independent of graphics profiling. No new renderer FPS gain is claimed.

## Exact 0.8 build identity

APK SHA-256: ea4e382a74b23eb7f9f133746dc8553aaa5588475d4a0ca8386cfff55ff1f1cf

- libmain.so Build ID: 66e67bb0ce8836f11571ab161f4b96afebb6a821
- libSDL2.so Build ID: 66fee9cd9d62da0387e5e2a318bf143316c7778d

APK and unstripped-symbol Build IDs agree. Both ARM64 libraries retain 16 KiB ELF
alignment; APK alignment and v2 signing checks pass with the existing Next/Port
certificate. Android versionCode is 24 and versionName is 0.8.
