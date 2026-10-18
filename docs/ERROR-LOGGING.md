# Optional error capture — 0.8

The original unexpected mid-race exit remains unresolved. This feature collects
better evidence; it is not presented as a gameplay crash fix.

## Use

Diagnostics → Record error logs → Play. The setting is off by default and
independent of performance recording. After an exit, reopen the launcher,
Diagnostics → Export error logs ZIP, choose a destination and share that file.
No files are uploaded automatically. No original game files are included.

## Collected evidence

- metadata.json: app/version code, device/Android version, process/session times,
  graphics choices and controller bindings; no serial or account identifier.
- lifecycle.log: game start/resume/pause/activity closure. An incomplete session
  alone does not prove a crash; closure alone does not prove a normal race end.
- engine.log and one rotated predecessor: info/warning/panic messages written by
  the native engine, excluding verbose debug/trace calls; 2 MiB each.
- logcat.log and one predecessor: warning/error lines filtered to this app's
  process, starting at capture setup; 1 MiB each. Some devices may refuse the
  stream, in which case that limitation is recorded rather than crashing setup.
- java-exception.txt: up to 256 KiB of an uncaught Java exception. The platform's
  original uncaught handler is always delegated to, even when capture is off or
  writing fails.
- native-signal.txt: best-effort signal/code/fault address/program counter. The
  signal handler uses only stack formatting and write, then preserves/chains
  Android's existing handler and original signal context. No signal-time memory
  allocation, stdio formatting, mutexes or stack unwinding is attempted.
- native-maps.txt: a bounded snapshot of this process's native memory mappings
  at engine startup, for relating the signal PC to the matching native library.
- exit-*.json: system-reported process reason/status and memory readings, matched
  to captured sessions by PID and timestamp. Android 11/API 30+ only, and the OS
  keeps a limited history. Events can indicate exit, signal, Java/native crash,
  ANR or memory pressure; a process exit is not automatically a gameplay crash.
- exit-*.trace.pb / .trace.txt: available system trace, at most two 1 MiB traces
  per session. Android 12/API 31+ can supply native tombstones as protobufs;
  other available traces are text. Missing or capped traces are noted in JSON.

The latest 12 session directories are retained; native logs and app logs rotate.
ZIP export uses an allowlist and per-file limits. Logs may include local paths,
app messages or text printed by the app; review before sharing. No READ_LOGS,
root access or broad device-log collection is added. Android 7–10 still get the
app/native capture paths but not ApplicationExitInfo history. SIGKILL, sudden
power loss or storage failure may leave little or no direct crash evidence.

## Reporting correction

The inherited Android OS handler logged a signal and called _exit(128+signal).
That bypassed normal fatal-signal termination/reporting. Version 0.8 leaves
Android's handlers intact when error capture is off; with capture on, the minimal
marker chains to the previous handler rather than swallowing the crash.
This reporting correction does not establish the cause of the reported race exit.

## Tests and symbols

`python3 test/android/test_error_capture.py` compiles the production native logger
on the host, verifies disabled capture/rotation/stop, fatal signal termination and
previous-handler invocation with its signal context. It does not run debuggerd.
`python3 test/android/test_error_export.py` exercises production Java storage,
exception delegation, limits, retention, ZIP contents, raw trace bytes and path
containment. Android logcat permissions, exit-history availability and real crash
export remain device checks. No crash is intentionally triggered in the normal UI.

Native trace analysis needs the exact matching build's unstripped ARM64 library;
keep its Build ID and symbols with the test APK. The maintainer build stores that
symbol archive separately from the APK and private signing key.

Official API references:
- https://developer.android.com/reference/android/app/ActivityManager#getHistoricalProcessExitReasons(java.lang.String,int,int)
- https://developer.android.com/reference/android/app/ApplicationExitInfo#getTraceInputStream()
- https://developer.android.com/ndk/guides/debug
