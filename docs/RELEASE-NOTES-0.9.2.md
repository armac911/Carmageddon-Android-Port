# 0.9.2 test build — languages and launcher cheats

Install over 0.9/0.9.1 without uninstalling or clearing storage. The package and
signing identity are unchanged; imported game data, saves and controls are retained.
The stable public 0.9 release is retained while this candidate is tested.

## Languages

Open **Language** in the launcher and explicitly choose English, Español,
Italiano or Français. English is the default even if Android uses another language.
The launcher updates immediately; exit the current game and press Play to change
its language. Switching languages while a game/import is active is blocked.

197 source phrases have Spanish, Italian and French entries. Coverage includes
launcher controls/help, recognised main/options/difficulty/race-start buttons
and 25 core indexed game messages/prompts. Captions are drawn into existing hit
areas; game assets, saves and original text files are never overwritten.
HUD text retains native font-change commands and uses original palette glyphs
with composed accents. Unknown text/artwork falls back to the imported original.

This first translation is intentionally documented as partial: some baked artwork,
other captions, race/car descriptions, pickup names and technical error details
remain original. Voice tracks and videos are unchanged. Text was drafted with AI
and needs native-speaker review; it is not an official game translation.

## Cheats

The previously added Cheats source is now included in an APK. Master switch,
invulnerability, free repairs and freeze timer all default off. Enable the master
and select helpers before Play. They last throughout single-player races in both
editions, survive timed pickup expiry and clear at race exit. Free repairs require
holding Repair. Normal progress/rewards still save. Network games ignore these helpers.

## Device test checklist

1. Upgrade normally. Confirm Language is English and cheats/diagnostics remain off
   unless you previously enabled them yourself.
2. Try each language, restart via Play, and inspect main menus, difficulty choices,
   repair/recovery costs and HUD accents (é, ñ, ç, à). Check both 4:3 and 16:9.
3. Return to English, restart and verify original text/menu appearance. Check
   both Carmageddon and Splat Pack, save/load and controller mapping.
4. Try the cheat master off, then each helper on in a race; finish/abandon and start
   another race. Confirm holding Repair works and switching cheats off restores play.
5. Continue the previous candidate's music, cockpit mirror and exact GOG-installer
   tests described in [0.9.1 notes](RELEASE-NOTES-0.9.1.md).

Native/Java regression tests and ARM64 build/signature/alignment checks pass.
No physical-device or native-speaker validation is claimed for this build.

## Known performance issue

Cockpit view can show a substantial FPS drop. The restored mirror adds a second camera render and readback/composition work; its exact share of the reported drop has not yet been profiled. Mirror quality/refresh controls are not included in 0.9.2. Use the external view as the current workaround.
