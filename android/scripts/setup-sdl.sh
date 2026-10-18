#!/usr/bin/env bash
# Downloads SDL2 source into android/app/jni/SDL so the gradle build can compile it.
set -euo pipefail

SDL_VERSION="2.30.10"
URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz"

cd "$(dirname "$0")/.."

if [ -d app/jni/SDL ] && [ -f app/jni/SDL/CMakeLists.txt ]; then
    echo "SDL2 already present in app/jni/SDL — nothing to do."
    exit 0
fi

mkdir -p app/jni/SDL
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading SDL2 ${SDL_VERSION}..."
curl -fsSL "$URL" -o "$TMP/SDL.tar.gz"

echo "Extracting..."
tar xzf "$TMP/SDL.tar.gz" -C app/jni/SDL --strip-components=1

echo "SDL2 ready at app/jni/SDL"
