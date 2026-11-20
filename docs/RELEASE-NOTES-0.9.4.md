# 0.9.4 combined test candidate

Includes the 0.9.3 control tuning and Cianid's PR #5 at
`c098178137a10c321c3b4b6cfee648f145fcfd75`. This APK uses version code 29,
package `com.androgaming.carma.next` and the existing maintainer signing key.
Install over the earlier maintainer APK without uninstalling.

## Check on S24 FE and Retroid Flip 2

- Start with the default controls. Drive the same car/track with stick, D-pad
  and touch. Check light/full RT/LT, braking to a stop and reverse, remaps,
  steering return and the tuning reset.
- In Graphics, compare Classic 4:3, Widescreen 16:9 and Full screen — fit device.
  Check centred HUD, original menus, map mode and external/cockpit views.
- Check cockpit edges, mirror, camera wobble after impacts, shadows and FPS.
  Device-fit renders more pixels on wider devices; no FPS gain is claimed.
- Check a reduced game screen size if you use that original setting. Ensure
  the 3D world, sky and borders agree in position.
- Load/save normally. Car stealing still needs original rank thresholds,
  eligibility and a 50% chance; it does not unlock every wrecked opponent.
- Capture optional error/performance ZIPs if anything fails. Logs remain off
  by default. The earlier cockpit FPS limitation is not claimed fixed.

## Validation scope

Automated input, analog engine-force, gear, mirror/GLES, aspect sizing, cockpit
edge/clipping, shared-parent offset readback and car-steal eligibility checks
pass. Translation and error-export checks are retained. These are synthetic
host/GLES tests, not physical-device driving or long-career validation.
The contributor reports Motorola Edge 50 Ultra testing of the PR's original
build. This combined 0.9.4 build still needs maintainer testing.

Cianid retains credit for PR #5. Their PR discloses Claude Code assistance;
ChatGPT/Codex assisted with integration, regression checks and packaging.
