# Changelog

This summarizes the major AndroGaming milestones. Detailed historical development
notes remain in [docs/DEVELOPMENT-HISTORY.md](docs/DEVELOPMENT-HISTORY.md).
Versions group related work rather than documenting every intermediate tweak.

## 0.9 — Splat Pack launcher integration

- Expose Dethrace's existing full Splat Pack support through a Home game selector.
- Add edition-aware ZIP checks and transactional imports into separate `game/`
  and `splatpack/` roots. Original data/saves remain in place; the expansion has
  its own saves/configuration. Reject mismatched or demo expansion data.
- Prevent switching/importing while the game activity is open; remember the
  import target through Android's document picker and label both diagnostic logs.
- Fix the nonfatal USB HID receiver-registration exception seen on Android 16:
  protected system broadcasts and private permission replies use separate filters
  with explicit Android 13+ export flags. Controller assignments are unchanged.
- Add structural import/save-preservation, native edition-detection and receiver
  registration tests. Actual Splat Pack assets and physical-device playtesting
  are still needed; no commercial expansion content is bundled.
- Review the 0.8 capture: normal exit, status 0, no native-signal or Java crash
  record; 57.9 FPS during the captured early racing interval at 16:9 High/Far.
  The earlier intermittent exit is not declared fixed.

## 0.8 — pre-release crash investigation

- Return the displayed release version from the 1.0 candidate to 0.8 after a
  reported mid-race exit. Keep a higher Android build code for in-place updates;
  no save/data migration or gameplay-feature rollback is performed.
- Add independent opt-in error capture and Android document-picker ZIP export,
  off by default, with bounded files and retention of the latest 12 sessions.
- Record engine messages, app warnings/errors, lifecycle/settings metadata and
  Java exception stacks; recover app process-exit reasons and available OS traces.
- Replace the inherited fatal-signal handler's immediate _exit with an optional
  async-signal-safe marker chained to Android's existing reporting path. The old
  handler could suppress normal native crash reporting. This fixes reporting,
  not the currently unidentified gameplay crash.
- Add host regression checks for opt-out/default exception delegation, log limits,
  rotation, ZIP allowlisting/containment, native fatal status and handler chaining.
  Physical-device crash/export verification is pending.

## Earlier 1.0 candidate — held for investigation

- Prepared from the revised 0.2 code after maintainer testing on Galaxy S24 FE and
  Retroid Pocket Flip 2: multiple completed races, car upgrades and stage 2
  progression; installation/import, saving/loading and controller defaults/remapping.
- Added player-focused setup, requirements, tested-device evidence, limitations,
  AI disclosure, component credits and contributor/release documentation.
- Keep package/signing compatibility with 0.1.15+; no new gameplay/rendering change
  relative to the tested revised 0.2. Distributed APK remains development-signed.

## 0.2 — complete driving controls and remapping

- Add the requested trigger/shoulder/face-button driving controls, Select map,
  R3 cockpit/external camera and right-stick forward-gear overrides.
- Add persistent launcher remapping, optional unassigned inputs, secondary
  acceleration binding, steering selection and restore-defaults.
- Combine touch/controller actions through loaded game key bindings; clear held
  input on focus loss/disconnection and test reconnect/menu behavior.
- Add ARM64 NEON transparent-pixel row merging to reduce readback CPU-copy work.
  Exact-output tests pass; its device FPS improvement remains unmeasured.
- Make native version metadata follow VERSION changes during incremental builds.

## 0.1.18–0.1.19 — texture filtering and cross-GPU correctness

- Replace final-image blur with bilinear filtering in 3D texture space, quantized
  through a palette lookup cube. Preserve sharp HUD/menus, cutouts and blend tables.
- Correct GPU blend-snapshot row addressing; a varying-background regression
  reproduced 56/64 incorrect pixels before the fix.
- Request high-precision GLES geometry/clipping and 24-bit Android depth storage.
  The maintainer subsequently confirmed the Flip 2 shadow-flicker fix.
- Make the launcher AndroGaming logo/credit open androgaming.com.

## 0.1.15–0.1.17 — launcher and presentation controls

- Introduce 60/120/144/uncapped frame pacing across menu and race presentation.
- Add the dark AndroGaming launcher with separate graphics, controls and
  diagnostics pages; branding remains outside gameplay.
- Set the app name/icon and make diagnostics/overlays opt-in by default.
- Establish the current com.androgaming.carma.next application/signing identity.
  Earlier com.androgaming.carma builds remain separate installations.

## 0.1.10–0.1.14 — measured rendering work reduction

- Add conservative off-screen model rejection and reduce redundant renderer work.
- Add exportable per-frame profiling with settings, device/driver and sensor
  metadata to distinguish simulation, copying and GPU synchronization costs.
- Read only canvas-resolution pixels required by CPU composition, batch eligible
  shadow work, keep blend snapshots on GPU and combine deferred shadow/main
  layers at the next required readback. Preserve fallback CPU-effect boundaries.
- Exercise production GLES functions against eager/reference composition at
  1×/2×/3×, including overlap, transparency, depth and CPU edits. Synthetic shadow
  flushes reduce from six to one, and shadow/main readbacks from two to one.
  Scene-dependent captures and synthetic counts are not universal FPS benchmarks.

## 0.1.1–0.1.9 — playable Android foundation

- Improve installation/data import, lifecycle/input handling and menu rendering
  on top of the inherited Android integration.
- Fix cursor/sprite origin memory access and menu/viewport pointer mapping;
  verify clipping/strides with sanitizer-backed regression fixtures.
- Implement native 4:3/16:9 canvas/layout support, retaining original HUD/menu art.
- Add Android text entry, combined touch actions, haptics, a touch-layout editor,
  automatic touch/controller presentation switching and graphics/steering options.
- Disable unavailable Network Game navigation and guard its startup path.

## Upstream foundation

The engine, game reimplementation and initial Android/GLES port precede these
iterations. Credit belongs to Dethrace, the inherited Android fork and their
contributors; see README and THIRD-PARTY-NOTICES. Complete original Git history
was not preserved in the restored source snapshot.
