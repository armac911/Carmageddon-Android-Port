#!/bin/sh
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
exec sh "$HERE/../Carmageddon/launch.sh" --probe
