# Validation — 0.9.2 test candidate

- Full Android ARM64 game and Java launcher assembleDebug passes with version
  0.9.2/code 27. Existing application ID and signing certificate are retained.
- APK v2 signature, ZIP alignment and all three native ELF LOAD alignments
  (16 KiB or higher) pass. Matching unstripped native symbols are retained.
- APK SHA-256: `26a367b46adff16baac02c6cc4b209eaf1bbd2237f86f986294179cca1566a17`.
- `test/android/test_localization.py` compiles production native catalogue/render
  code under AddressSanitizer/UBSan: all languages, unknown-key/language fallback,
  English passthrough, accented UTF-8, HUD font commands, width, clipping,
  sub-pixmap bounds and recognised menu caption overlays pass. Empty save-slot
  captions are checked for uppercase ASCII and the original 12-character limit.
- The same test compiles the generated Java catalogue and verifies English
  remains default under a French system locale, explicit selection, dynamic
  game-name templates and reset to English.
- Production cheat race-hook checks pass for defaults, all option combinations,
  repeated races, timed pickup expiry, cleanup and normal/network isolation.
- Production Java import tests pass for edition isolation, wrapped ZIPs, support
  merges, controlled installer routing and preservation of existing data/saves.
- This is an initial text translation, not complete localisation of all artwork,
  race/car descriptions, powerups, technical error details or voices/videos.
  Native-speaker review and physical-device checks of fonts, caption placement,
  4:3/16:9, both editions and switching back to English remain pending.
- Previous synthetic music/mirror results below apply to the inherited changes;
  this iteration does not claim new physical-device music/mirror validation.

# Validation — previously built 0.9.1 test candidate

- Import JVM tests pass for wrapped ZIPs, Max Pack edition isolation, matching
  support overlays, music selection, ambiguity rejection and transactional saves.
- Controlled installer-helper tests pass; the real built native reader rejects
  an invalid MZ input cleanly. No real Carmageddon installer was supplied.
- The native Inno Setup reader builds for Linux and Android ARM64. The ARM64 PIE
  uses the Android linker, platform shared libraries only and 16 KiB LOAD alignment.
- The full game builds for Linux and Android ARM64 with the restored mirror and
  streamed music. No game assets are needed for compilation.
- Synthetic production music decoding/mixing passes OGG, MP3, FLAC and WAV,
  case-insensitive discovery, volume, stop/restart and cleanup without a sound device.
- Production GLES readback/shadow tests pass. New mirror target tests pass
  separate colour/depth, bounded sub-pixmap copy, unchanged main scene/snapshot,
  restored winding/state and 1x/2x/3x settings using Mesa software GLES.
- Java extractor management uses Process APIs available at minSdk 24; timed
  polling avoids Process methods introduced at API 26.
- Physical-device music, reflected scene orientation, cockpit/widescreen placement,
  frame rate and exact `setup_carmageddon_2.0.0.63.exe` extraction remain pending.
- APK assembleDebug passes; version 0.9.1/code 26, expected package and label,
  matching 0.9 signing certificate, v2 signature and ZIP/ELF 16 KiB alignment pass.
- APK SHA-256: `de284e8025f9b9fa641034dc3994e8ade8dc628c9cc1b628a1b772537f786f4d`. Matching unstripped symbols are retained.

# Prior 0.9 validation

- Android ARM64 assembleDebug succeeds with JDK 17, Gradle 8.7, AGP 8.6.1,
  SDK/build tools 35, NDK 28.2 and CMake 3.22.1.
- Android metadata: com.androgaming.carma.next, versionCode 25, versionName 0.9,
  display name Carmageddon Android Port.
- Native engine version is androgaming-0.9. CMake now watches the VERSION file
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
crash cause is unresolved. The maintainer subsequently reports almost two hours
of stable 0.8 testing; the supplied S24 FE session records about 54 minutes
and a normal exit. This does not establish the earlier crash cause.
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


## 0.9 expansion checks

- test_game_content.py: production Java importer/root selection, original-save
  sentinel preservation, separate full-expansion import, wrong-edition/demo
  rejection, transactional staging cleanup and existing-install protection.
- test_game_detection.py: actual inherited native detector recognizes original
  Carmageddon and full Splat Pack markers/intro selection using synthetic files.
- test_usb_registration.py: actual registration helper uses legacy behavior on
  API 24/32 and separate exported system/private permission filters on API 33/36.
- Supplied 0.8 S24 FE session: normal exit/status 0, no Java/native crash record;
  one caught USB registration error is addressed here. Performance: 57.89 FPS in
  the captured early racing interval, not a full two-hour trace or 0.9 benchmark.
- No actual Splat Pack data was available to this build/test pass. Intro, first
  race, progression, save/load and game switching still require maintainer
  device testing. This build integrates the inherited engine's expansion path;
  it does not claim a new expansion-engine implementation or fix for the earlier
  intermittent exit.

## Exact 0.9 build identity

- APK SHA-256: `00463ab13c241cb625538bf9c021a3e44edfeeae56c26530b3673a09f683023c`.
- libmain.so Build ID: `23d600021c79dc9f2ebbf4a94c2d24ed014d0e87`; minimum ELF load alignment 16384 bytes.
- libSDL2.so Build ID: `90e4dfdef746306763a5ea1b3510be04694332e8`; minimum ELF load alignment 16384 bytes.
- Matching unstripped libraries are retained in the 0.9 native-symbols ZIP.
