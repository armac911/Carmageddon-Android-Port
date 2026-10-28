# Input tuning — 0.9.3 test candidate

Launcher: **Controls → Deadzones & sensitivity**. Changes apply on next Play.
Restore input tuning defaults resets only these sliders, preserving remaps,
layout, saves and graphics preferences. Existing users also receive the new
response defaults when no tuning overrides exist.

| Setting | Default | Range | Meaning |
| --- | --- | --- | --- |
| Stick steering deadzone | 24% | 0–50% | Ignore motion close to the horizontal stick centre. Applies to the selected steering stick. |
| Stick steering sensitivity | 85% | 25–150% | Lower softens the centre; full analog travel still reaches full steering. |
| Touch steering deadzone | 8% | 0–50% | Ignore movement around the steering pad centre. |
| Touch steering sensitivity | 85% | 25–150% | Analog touch response curve, preserving full travel. |
| Trigger deadzone | 10% | 0–50% | Ignore light trigger movement. |
| Pedal sensitivity | 100% | 25–150% | Curve for axes mapped to acceleration/brake, including secondary acceleration. |
| Action stick activation | 50% | 20–90% | Stick-direction action threshold, including gears and stick directions remapped as pedals. Actions release at half the threshold to prevent chatter. |
| Digital steering strength | 85% | 25–100% | Maximum input for D-pad and Digital mode. Unlike analog sensitivity, limits maximum steering. |
| Steering build time | 250 ms | 0–1000 ms | Centre to full input; applies to all steering sources. |
| Steering return time | 180 ms | 0–1000 ms | Full input to centre. Reversal returns to centre before building the other direction. |
| Pedal build time | 120 ms | 0–500 ms | Rise time for all pedals, including touch/buttons. Release is immediate. |

Deadzones are rescaled over the remaining travel. Sensitivity uses a power curve
before the engine's existing quadratic steering curve. Input smoothing uses
elapsed game-frame milliseconds, not a per-frame constant. Zero response times
select immediate input. In Digital mode, touch/stick sensitivity curves do not
alter the on/off output; deadzone, digital strength and steering times do.

Camera, map, repair, recover, handbrake and menu buttons remain digital actions.
Action deadzone applies when one is mapped to an axis; there is no artificial
button deadzone. Keyboard-only input retains the original game behavior.
The original car grip, mass and tyre settings are unchanged.

## Test on device

1. Install over the existing app without uninstalling; check saves/imports.
2. Use the same car, upgrades and track for left stick, D-pad and touch runs.
3. Compare coasting corners with acceleration through the same corner.
4. Check gentle RT/LT presses versus full travel, braking to a stop and reverse.
5. Check X/touch gas, simultaneous steering/braking, and remapped pedal inputs.
6. Change each slider, restart Play, and confirm behavior; reset tuning afterward.
7. Check neutral stick, opposite directions, pause/resume and controller disconnect.
8. Compare steering at 60 and higher caps. Record the control source and effective
   settings with any report; opt-in diagnostics include them.

Automated tests do not establish driving feel or eliminate a potential underlying
physics issue. Physical-device validation is pending. New tuning text is English.
