# 0.9.1 test build — easier import, music and cockpit mirror

This is a device-test candidate. The public 0.9 release is retained.

- Select a ZIP of your PC installation without rearranging its folders. The
  importer searches for the selected game's DATA, finds adjacent/enclosing
  MUSIC, and overlays matching GOG `__support/app/CARMA` or `CARSPLAT` files.
  Unrelated files and the other game's assets are not installed. Ambiguous
  archives containing multiple complete copies are rejected.
- A bundled native innoextract reader handles supported self-contained GOG
  Inno Setup EXE installers. Windows installer code is never executed.
  The exact `setup_carmageddon_2.0.0.63.exe` has not been supplied for testing.
  Installers requiring additional BIN/RAR volumes are not supported by this
  initial picker; use a ZIP of the installed game for those distributions.
- Added real streamed music playback and OGG/Vorbis decoding. OGG, MP3, FLAC
  and WAV are supported. Case-insensitive discovery also covers existing 0.9
  MUSIC imports. Playback follows music volume, cutscene start/stop and the
  existing audio pause/resume path.
- Restored the cockpit mirror rendering pass. It uses an isolated GPU target,
  reflected winding and CPU composition that accounts for the widescreen HUD
  sub-view. The original third-person view has no added mirror overlay.
  A second camera costs rendering work; no FPS gain is claimed.

Install over 0.9 without uninstalling or clearing storage. Existing game data,
saves, controls and settings are preserved. A new import never replaces an
existing installation. Use a separate clean Android profile/device to test a
fresh import while retaining your main saves.

Device checks: import a wrapped ZIP, import the named GOG EXE, confirm music
and its volume slider/pause/resume, check the cockpit mirror while driving in
4:3 and 16:9 at each 3D resolution, and switch to third person/map and back.
Test original Carmageddon and Splat Pack separately. Exact-installer and
physical-device gameplay results remain pending.
