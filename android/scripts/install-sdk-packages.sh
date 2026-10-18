#!/usr/bin/env bash
set -euo pipefail
sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [[ -z "$sdk" || ! -x "$sdk/cmdline-tools/latest/bin/sdkmanager" ]]; then
    echo 'Install Android command-line tools and set ANDROID_HOME first (see START-HERE.md).' >&2
    exit 1
fi
"$sdk/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$sdk" \
    'platforms;android-35' 'build-tools;35.0.0' 'platform-tools' \
    'ndk;28.2.13676358' 'cmake;3.22.1'
