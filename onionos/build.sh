#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${SDL_PREFIX:?Set SDL_PREFIX to the patched SDL2 install prefix}"
: "${MIYOO_SDK_LIB_DIR:?Set MIYOO_SDK_LIB_DIR to steward-fu/sdl2/mini/lib}"
CMAKE=${CMAKE:-cmake}
BUILD=${BUILD:-"$ROOT/build-miyoo"}
"$CMAKE" -S "$ROOT" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/miyoo-toolchain.cmake" \
    -DMIYOO_SDK_LIB_DIR="$MIYOO_SDK_LIB_DIR" \
    -DCMAKE_BUILD_TYPE=Release -DDETHRACE_MIYOO=ON \
    -DSDL2_INCLUDE_DIR="$SDL_PREFIX/include/SDL2" \
    -DSDL2_LIBRARY="$SDL_PREFIX/lib/libSDL2.so"
"$CMAKE" --build "$BUILD" -j "${JOBS:-4}"
