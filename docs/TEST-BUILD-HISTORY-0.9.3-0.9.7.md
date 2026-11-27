# Historical test-build notes: 0.9.3–0.9.7

These are archived development checkpoints, including superseded approaches and then-pending checks. For final behavior and confirmed testing, see the current changelog and 0.9.7 release notes.

## 0.9.7 — centred widescreen map (test candidate)

- Compose the complete original map screen at its original 4:3 proportions,
  centred at full available height in 16:9 and device-fit canvases. Clear side
  bars and force CPU presentation so the race view cannot show through.
- Scale map artwork, checkpoints, car blips, timer and live preview together,
  preserving their alignment. Original 4:3 and low-resolution paths stay intact.
- Retain the 0.9.6 cockpit fix, confirmed by the maintainer. Mirror behavior
  remains cockpit-only, matching the inherited engine; no external mirror added.
- Physical-device map confirmation pending.

## 0.9.6 — remove artificial cockpit side extensions (test candidate)

- Remove the cockpit side-extension pass. The row-colour extension in 0.9.4
  caused streaks; the black fill in 0.9.5 replaced them with rectangular blocks.
  Draw the original cockpit image at its native size and leave the already
  rendered world visible outside its bounds and through transparent windows.
- Preserve the controls and other PR #5 fixes. Regression checks exercise the
  production cockpit copier with camera wobble, clipping and guard bytes,
  requiring untouched world pixels on both sides of the original artwork.
- Physical-device visual confirmation remains pending.

## 0.9.5 — cockpit side streaking (test candidate)

- Replace stretched cockpit edge colours with palette-black opaque side fill,
  retaining the original cockpit artwork and transparent window rows. This
  addresses the horizontal streaking reported on S24 FE and Retroid Flip 2.
- Preserve the PR #5 fixes and 0.9.3 controls. The maintainer reports partial
  triggers and other controls working on Flip 2 with the 0.9.4 candidate.
- Regression checks require identical black extensions for differently coloured
  edge pixels, with transparent windows, artwork bounds and clipping preserved.
  Physical-device confirmation of this visual fix is still pending.

## 0.9.4 — PR #5 and control improvements (combined test candidate)

- Integrate Cianid's PR #5 on top of the 0.9.3 input changes, preserving
  controller remapping, all tuning sliders and proportional pedals.
- Apply BRender sub-pixmap destination offsets during GPU readback, fixing
  off-centre views when the render rectangle is inset.
- Add an optional device-fit aspect mode, migrate existing aspect preferences,
  and retain centred menus/HUD. Wider rendering may increase GPU cost.
- Improve wide cockpit composition, projection offset and opaque edge extension
  while preserving transparent window rows.
- Restore original strength 1–5 car-stealing thresholds and clamp the strength
  before indexing, removing the strength-5 out-of-bounds access.
- Credit: Cianid authored these fixes with disclosed Claude Code assistance;
  AndroGaming's integration/build/regression work used ChatGPT/Codex assistance.
- Device testing of this combined APK is pending; PR #5 remains open for review.

## 0.9.3 — adjustable input response (unreleased test candidate)

- Smooth stick, D-pad and touch steering with elapsed-time rate limits, including
  separate steering build/return times. Original vehicle grip is unchanged.
- Add launcher sliders for stick/touch deadzones and sensitivity, trigger
  deadzone/pedal sensitivity, digital steering strength and action-stick
  activation (including gear changes), with one-tap tuning reset.
- Make acceleration and brake axes proportional, including remapped axes;
  button/touch pedals build progressively and release immediately.
- Remove an inherited abort in the unused analog throttle path; fix integer
  division and accumulation in analog brake pressure. Scale the existing
  traction limiter by pedal position rather than overriding a small press.
- Include effective tuning in opt-in diagnostic/error exports. Automated checks
  cover partial input, reversals, release, reverse selection and remapping.
- Requires driving validation on physical devices. New tuning labels/help use
  English; existing language support remains available. Multiplayer and the
  known cockpit FPS limitation are unchanged.

