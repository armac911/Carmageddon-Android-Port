#!/usr/bin/env bash
# Build the pinned native Inno Setup reader; Windows installer code is never run.
set -euo pipefail
cd "$(dirname "$0")/../.."
sdk="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
ndk="${ANDROID_NDK_HOME:-${sdk}/ndk/28.2.13676358}"
cmake_bin="${CARMA_CMAKE:-${sdk}/cmake/3.22.1/bin/cmake}"
if [[ ! -x "$cmake_bin" ]]; then cmake_bin=cmake; fi
if [[ ! -f "$ndk/build/cmake/android.toolchain.cmake" ]]; then
    echo 'Install NDK 28.2.13676358 or set ANDROID_NDK_HOME.' >&2; exit 1
fi
build=android/.installer-build/arm64-v8a
"$cmake_bin" -S android/installer -B "$build" \
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
    -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
"$cmake_bin" --build "$build" --target innoextract --parallel "${CARMA_BUILD_JOBS:-2}"
mkdir -p android/app/libs/arm64-v8a
cp "$build/out/libinnoextract.so" android/app/libs/arm64-v8a/libinnoextract.so

"$ndk/toolchains/llvm/prebuilt/$(case "$(uname -s)" in Darwin) echo darwin-x86_64;; *) echo linux-x86_64;; esac)/bin/llvm-strip" --strip-unneeded android/app/libs/arm64-v8a/libinnoextract.so
