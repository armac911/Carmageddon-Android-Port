# Changelog

This summarizes the major AndroGaming milestones. Detailed historical development
notes remain in [docs/DEVELOPMENT-HISTORY.md](docs/DEVELOPMENT-HISTORY.md).
Versions group related work rather than documenting every intermediate tweak.

## Unreleased — graphics options (development build, not device-tested)

- Real supersampling: High and the new Ultra (4×) 3D resolution now average
  every rendered sample per screen pixel instead of picking one, including
  before FSR/GSR upscaling on wide screens. Ultra steps down automatically
  when the GPU's maximum texture size is too small.
- Smooth texture filtering gains palette-preserving mipmaps with trilinear
  filtering, optional 2×/4× anisotropic filtering, and ordered dithering
  before mapping back to the 256-colour palette. Palette fades reuse the
  colour lookup table instead of rebuilding it every step.
- New launcher options: FXAA edge smoothing (3D only, menus and HUD stay
  crisp), RCAS sharpening levels, Sharp/Smooth (Scale2x) menu and HUD
  scaling, wider field of view, brightness, vivid colour and CRT scanlines.
  All default to the original look.
- The final screen shader is compiled with only the features your settings
  use. At defaults it does the same texture reads as 0.9.2 (and drops
  0.9.2's unused smoothing path); SSAA averaging runs only on frames where
  the 3D image is larger than the screen.
- Graphics page: a Compatibility section restores 0.9.2 behaviour for the
  always-on changes (supersampling average, mirror render size, track
  visibility test), and "Reset graphics to defaults" resets the whole page.
- Field of view now widens only the external chase camera; cockpit view
  and the rear-view mirror keep the original angle (the cockpit camera must
  match its artwork, and a wider mirror only added cost).
- Track columns outside the camera's view are skipped before they are
  traversed and submitted. The test is provably conservative for any camera
  pitch and roll; the original box selection kept up to ~3× more columns on
  wide screens and wide fields of view.
- New Crisp texture filtering: original solid texels up close with
  anti-aliased texel edges, plus mipmaps against distant shimmer.
- Cockpit mirror: drawn at 1× instead of the full 3D resolution (it is only
  shown at canvas resolution), with new Half rate and Off-at-race-start
  options to cut its cost further.
- Tilt steering (three sensitivities) using the gravity sensor; it
  re-centres on the current hold at race start and on resume. Touching the
  steering pad takes priority. With a controller, tilt is added to the stick
  by default (phone in a controller grip), or can be ignored.
- Optional enhanced effects (Graphics, off by default): tyre smoke from
  every sliding wheel on tarmac plus skid marks that last three times
  longer, and impacts with more sparks and twice the car fragments. They
  extend the game's own effect systems, so replays and fog treat them like
  the originals, and they use no extra random numbers for tyre smoke.
- Memory: textures released by the game (every track change, splash
  screens) now free their GPU memory; BrBufferClear was a stub, so they all
  leaked. Released renderer material data is freed too. The mirror's render
  targets and the CPU readback buffers are sized for what Android uses
  (about 25 MB GPU and 14 MB CPU less at Ultra).
- Frame time: readbacks and blend snapshots cover only the drawn area (the
  mirror was reading the whole screen); shadow and crash-deformed models
  reuse their GL buffers instead of recreating them every frame; no GL
  queries on palette changes; the colour lookup table is not rebuilt from
  dark fade palettes; textures loaded during a black loading screen get
  their mipmaps during the fade instead of on the first race frame; the
  snapshot is uploaded only when it changed; settings read every frame are
  cached; the touch overlay redraws only when it changes; UI polling stops
  while the game is paused.
- Ukrainian launcher language. In-game text stays in English because the
  game's bitmap fonts have no Cyrillic letters.
- tools/check_apk_privacy.py checks a built APK for personal values,
  absolute user paths and unstripped native libraries before sharing.
- Crash vibration now scales with the size of the hit (velocity change) on
  the phone, or rumbles a controller in use. The previous impact check
  compared the car's internal speed unit against 5 and could not trigger.

## 0.9.2 — optional languages and launcher cheats (test candidate)

- Add explicit English/Spanish/Italian/French selection in the launcher. English
  is always the default, independent of Android language. Translations live in
  an immutable catalogue; imported DATA/MUSIC and saves are never rewritten.
- Localize launcher controls, recognised game menu buttons and core race HUD
  messages; compose Latin diacritics with the existing font/palette, preserving
  HUD font-switch commands and falling back to original text/artwork elsewhere.
  Original voices/videos remain unchanged. Translations need device/native-speaker review.
- Add a Cheats page with a master switch and separate invulnerability, free
  repair and frozen race timer options. All switches default off; selections
  apply on the next Play and are remembered when the master switch is disabled.
- Apply selected cheats throughout single-player races for both editions,
  restore them after timed pickups expire, and clear their flags at race exit.
  Normal powerups remain unchanged when disabled. No launcher cheats in network games.
- Production race-hook regression checks cover defaults, all combinations,
  repeated races, pickup expiry, cleanup and master-off/network isolation.
  These cheats are now included in the 0.9.2 APK. The prior 0.9.1 APK predates them.

- Known issue: Cockpit view can show a substantial FPS drop. The restored mirror adds a second camera render and readback/composition work; its exact share of the reported drop has not yet been profiled. Mirror quality/refresh controls are not included in 0.9.2. Use the external view as the current workaround.

## 0.9.1 — import, music and cockpit mirror (test candidate)

- Automatically locate the selected game's DATA and MUSIC inside wrapped ZIPs,
  select between CARMA/CARSPLAT, and merge matching GOG support files.
- Bundle a pinned native innoextract reader for self-contained GOG offline EXEs;
  no Windows executable code is run. Exact named-installer/device testing pending.
- Replace empty inherited music start/stop hooks with streamed playback; add
  OGG/Vorbis decoding alongside MP3, FLAC and WAV, volume and lifecycle handling.
- Restore cockpit rear-view rendering with isolated GPU colour/depth targets,
  reflected winding and widescreen-aware CPU composition. Third-person has no
  new mirror overlay. Additional camera rendering has a performance cost.
- Preserve existing installs/saves and extend import/audio/GLES regression checks.
  This candidate does not replace the public 0.9 release until device validation.

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
