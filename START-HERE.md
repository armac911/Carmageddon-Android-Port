# Carmageddon Android Port 0.9 — Splat Pack

Install over 0.8 or the earlier 1.0 candidate without uninstalling. The app/signing
identity is retained, with versionCode 25. Original imported data and saves stay
in their existing location.

1. In Home, press **Game: Carmageddon**, then choose **Splat Pack**.
2. Import a ZIP made from your own full Splat Pack installation: `DATA/` directly
   at the ZIP root, with optional `MUSIC/`. In a Max Pack layout this is usually
   `CARSPLAT/DATA/`, merged with matching support DATA files if supplied.
3. Press **Play Splat Pack**. Its data/saves use a separate `splatpack/` folder.
4. Exit the game fully before switching back to Carmageddon in the launcher.

No game files need to be uploaded here. If import fails, share the error text
and a file listing rather than the commercial game assets. First test the intro,
first race, save/load, and switching back to the original game with its saves.
Splat Pack gameplay on real data has not yet been verified in this build.

Error and performance capture remain independent opt-ins; defaults are off.
The submitted 0.8 error log records a normal exit, not a crash. The earlier
intermittent exit remains unexplained, so the 1.0 milestone stays on hold.

See README, CHANGELOG, VALIDATION and docs/ERROR-LOGGING.md for details.
