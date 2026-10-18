# DethRace — Android port

Native Android build of [DethRace](https://github.com/dethrace-labs/dethrace) (a clean-room reimplementation of Carmageddon 1).

## What's here

A Gradle / NDK project under `android/` that:

- Bundles SDL2 (2.30.10, fetched via `scripts/setup-sdl.sh`) as the windowing/input layer.
- Cross-compiles the dethrace C engine (off the upstream `opengles` branch) to `arm64-v8a`.
- Renders via OpenGL ES 3 (Adreno-tested on the Galaxy S25 Ultra).
- Adds a touch overlay with steering arrows, gas/brake pedals and a pause button — visible only during a race.
- Pauses miniaudio when the activity goes to background so other apps stop hearing your engine.

The desktop builds are untouched — every Android-specific tweak is gated behind `#ifdef __ANDROID__` or `if(ANDROID)` in CMake.

## Requirements

- Android Studio with SDK 34 + NDK r26 (`26.1.10909125` is what was tested).
- A Carmageddon Max Pack (or original) install with its `DATA/` folder.

## Build

```sh
# 1. Pull SDL2 (one-time, ~15 MB download)
./android/scripts/setup-sdl.sh

# 2. Tell Gradle where your SDK is
echo "sdk.dir=$ANDROID_HOME" > android/local.properties

# 3. Build
cd android
./gradlew assembleDebug
```

APK will land at `android/app/build/outputs/apk/debug/app-debug.apk`.

## Install + game files

```sh
adb install -r android/app/build/outputs/apk/debug/app-debug.apk

# Push the DATA/ folder from your Carmageddon install:
adb push /path/to/CARMA/DATA \
    /sdcard/Android/data/com.dethrace.android/files/

# Plus the GoG support files (KEYMAP_*.TXT, OPTIONS.TXT, PATHS.TXT):
adb push /path/to/__support/app/CARMA/DATA/. \
    /sdcard/Android/data/com.dethrace.android/files/DATA/
```

## Controls

In landscape, during a race:

| Region | Action | DOS key sent |
|---|---|---|
| Bottom-left, left half | Steer ◀ | Keypad 4 |
| Bottom-left, right half | Steer ▶ | Keypad 6 |
| Bottom-right, upper | Brake / Reverse | Keypad 2 |
| Bottom-right, lower | Accelerate | Keypad 8 |
| Top-right corner | Pause / menu | Esc |

Touch zones are larger than their visuals — easier to hit with a thumb than the on-screen icons suggest.

## Limitations / known issues

- Arrow controls only — no weapons, lookback, etc. yet. Bring a Bluetooth keyboard for those.
- Aspect ratio is the original 320×200 (8:5). On 21:9 phones you'll see black pillars — necessary to keep the HUD intact.
- The `opengles` upstream branch is ~700 PRs behind `main`; bug fixes there don't carry over yet.
- Smacker intro videos seem fine but haven't been stress-tested.

## License

Same as upstream DethRace. Carmageddon assets are © Stainless Games — bring your own.
