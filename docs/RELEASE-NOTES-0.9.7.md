# Carmageddon Android Port 0.9.7

An [AndroGaming.com](https://androgaming.com) project. This pre-release brings together the control improvements and community rendering fixes tested since 0.9.2.

## Controls that are easier to tune

- Added launcher adjustments for controller-stick and touch deadzones/sensitivity, trigger deadzone, pedal sensitivity, digital steering strength and action-stick activation thresholds, including gear changes.
- Added separate steering build/return times and pedal build time, plus a one-tap tuning reset that preserves controller mappings and saves.
- Smoothed stick, D-pad and touch steering using elapsed time, keeping response consistent across frame rates. Original vehicle grip and tyre physics remain unchanged.
- Made trigger acceleration/braking proportional, including remapped pedal axes. Button/touch pedals build progressively and release immediately.
- Fixed inherited analog-throttle abort and brake-calculation errors that could turn partial braking into full braking. The existing traction limiter now respects pedal position.
- Improved input reset handling around pauses, disconnections and direction changes. Opt-in diagnostic exports include the effective tuning settings.

## Rendering, cockpit and map fixes

- Integrated [Cianid’s PR #5](https://github.com/armac911/Carmageddon-Android-Port/pull/5), correcting GPU readback offsets that could leave inset or reduced 3D views off-centre.
- Added **Full screen — fit device** alongside the existing aspect choices, with preference migration and centred menus/HUD.
- Corrected wide cockpit composition and projection alignment. Removed artificial cockpit edge extensions that caused horizontal streaks and black blocks on wide displays; the original cockpit artwork stays centred with the world visible beyond it.
- Fixed the map occupying only the left portion of a wide screen. The complete map screen now fits centrally at its original proportions with clean side bars. Checkpoints, car markers, timer and live driving preview scale together, and the previous race image no longer shows through.
- Fixed an out-of-bounds lookup in strength-5 car stealing while preserving the original eligibility, rank thresholds and chance rules.

## Validation and updating

The maintainer tested on **Samsung Galaxy S24 FE** and **Retroid Pocket Flip 2**, confirmed partial triggers and the revised controls, and approved the cockpit/map fixes through iterative testing. Cianid also reported testing PR #5 on **Motorola Edge 50 Ultra**.

Targeted automated checks cover input response at different frame rates, partial throttle/brake behavior, remapping, GPU readback offsets, cockpit transparency/clipping, proportional map composition and car-stealing bounds. Android packaging, signing and 16 KiB alignment checks passed. These checks do not establish a general FPS improvement or compatibility with every device.

Install `carmageddon-android-port-0.9.7-arm64.apk` over the existing maintainer build **without uninstalling** to retain imported data and saves. Android 7.0+ and ARM64 are required. You must supply compatible, legally owned PC game data; no original DATA or MUSIC is included. Matching native debug symbols and source archives accompany the release.

## Still open

Cockpit rendering can reduce FPS; the rear-view mirror remains cockpit-only. LAN/Wi-Fi multiplayer is not available. Audio mixing/disabled audio settings ([#3](https://github.com/armac911/Carmageddon-Android-Port/issues/3)) and controls obscured after resume ([#4](https://github.com/armac911/Carmageddon-Android-Port/issues/4)) remain open. Language support is partial, and the new control-tuning text is currently English.

## Thanks and AI disclosure

Thank you to **Cianid** for PR #5, testing and screenshots; **Draknu** for the display report in #2; **porkuskorpz** for steering/audio feedback in #3; **DentonTheFox** for display/resume reports; and everyone sharing device tests, logs and feedback. **Dejan / AndroGaming.com** directs the project and performs the maintainer’s device testing.

This work depends on **Dethrace and its contributors**, **mgrz18/dethrace-android**, and the third-party libraries credited in the [README](https://github.com/armac911/Carmageddon-Android-Port#credits-and-licenses). Thanks also to **Stainless Games**, the original creators of Carmageddon.

AndroGaming’s changes, integration, tests, documentation and packaging were developed with substantial **OpenAI ChatGPT/Codex assistance, including generated and edited code**. Cianid disclosed **Claude Code** assistance for PR #5. AI assistance does not replace contributor attribution or imply endorsement by the AI providers. See the README for full AI disclosure and license notices.
