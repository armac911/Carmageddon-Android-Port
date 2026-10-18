#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [[ -z "$sdk" ]]; then
    echo 'Set ANDROID_HOME to your Android SDK directory.' >&2
    exit 1
fi
command -v java >/dev/null
./android/scripts/setup-sdl.sh
printf 'sdk.dir=%s\n' "$sdk" > android/local.properties
cd android
./gradlew assembleDebug --no-daemon --console=plain "$@"
