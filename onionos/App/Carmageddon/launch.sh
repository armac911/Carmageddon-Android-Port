#!/bin/sh
# No firmware writes, swap, overclocking, or system-library replacement.
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "$HERE" || exit 1
mkdir -p logs game || exit 1
# Keep three runs. A single run is not truncated while diagnosing a crash.
mv -f logs/previous.log logs/older.log 2>/dev/null
mv -f logs/latest.log logs/previous.log 2>/dev/null
exec >logs/latest.log 2>&1
printf 'Carmageddon OnionOS experimental 0.1.1\n'
date -u
uname -a
cat BUILD-INFO.txt 2>/dev/null
cat /proc/meminfo
printf '\nCPU\n'
cat /proc/cpuinfo
printf '\nVideo and input\n'
cat /proc/fb /proc/bus/input/devices 2>/dev/null
printf '\nOnion version\n'
cat /mnt/SDCARD/.tmp_update/onionVersion/version.txt 2>/dev/null
export LD_LIBRARY_PATH="$HERE/lib:/config/lib:/customer/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DETHRACE_ROOT_DIR="$HERE/game"
# Driver autodetection: steward-fu uses "Mini", older ports may use "mmiyoo".
# Do not force SDL_RENDER_DRIVER=software: Mini's 2D blitter is the faster presenter.
export SDL_AUDIODRIVER=dummy
CHILD=
WATCHER=
STOPPED=
cleanup() {
    [ -z "$WATCHER" ] || kill "$WATCHER" 2>/dev/null
    if [ -n "$CHILD" ]; then kill "$CHILD" 2>/dev/null; wait "$CHILD" 2>/dev/null; fi
    for ui in $STOPPED; do kill -CONT "$ui" 2>/dev/null; done
}
trap cleanup EXIT
trap 'exit 130' INT TERM
# Suspend only an active MainUI; preserve an already-stopped process state.
for ui in $(pidof MainUI 2>/dev/null); do
    state=$(sed -n 's/^State:[[:space:]]*\([A-Z]\).*/\1/p' "/proc/$ui/status")
    case "$state" in T|Z|'') ;; *) if kill -STOP "$ui" 2>/dev/null; then STOPPED="$STOPPED $ui"; fi ;; esac
done
run() {
    "$@" &
    CHILD=$!
    (
        while kill -0 "$CHILD" 2>/dev/null; do
            printf '\nprocess sample pid=%s\n' "$CHILD"
            sed -n '/^VmPeak:/p;/^VmSize:/p;/^VmHWM:/p;/^VmRSS:/p;/^Threads:/p' "/proc/$CHILD/status" 2>/dev/null
            sed -n '/^MemAvailable:/p;/^MemFree:/p;/^SwapFree:/p' /proc/meminfo
            sleep 5
        done
    ) &
    WATCHER=$!
    wait "$CHILD"
    RESULT=$?
    kill "$WATCHER" 2>/dev/null
    wait "$WATCHER" 2>/dev/null
    WATCHER=
    CHILD=
    printf '\nExit status: %s\n' "$RESULT"
    cat /proc/meminfo
    return "$RESULT"
}
if [ "${1:-}" = --probe ]; then
    run ./miyoo-probe 30
    exit $?
fi
if [ ! -d game/DATA ] || [ ! -f game/DATA/GENERAL.TXT ]; then
    printf '\nMissing game/DATA/GENERAL.TXT. Copy your own PC DATA folder, preserving UPPERCASE names.\nRunning the data-free probe instead.\n'
    run ./miyoo-probe 10
    exit 4
fi
# First device build: low resolution, no cutscenes/audio/replay and less geometry.
# Own DATA may be modified by the original settings/save routines; use a copy.
run ./dethrace --debug=3 --fps=30 --no-signal-handler -lomem -nosound -nocutscenes -noreplay -yon 0.35 -simple 4
exit $?
