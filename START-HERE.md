# Carmageddon Android Port 0.8 — crash investigation

Version 1.0 is held after a reported unexpected mid-race exit. The cause is not
yet known. Version 0.8 keeps the driving/rendering features and adds opt-in error
capture. Its internal build code is higher than the 1.0 candidate, so install it
as an update without uninstalling or clearing storage.

1. Open Diagnostics and enable **Record error logs** (off by default).
2. Press Play and try the affected race. Performance recording is independent.
3. After an exit, reopen the launcher and choose **Export error logs ZIP**.
4. Save and share that ZIP with the device, settings and reproduction steps.

See [README.md](README.md), [error capture details](docs/ERROR-LOGGING.md),
[CHANGELOG.md](CHANGELOG.md) and [VALIDATION.md](VALIDATION.md).
Historical notes remain in docs/DEVELOPMENT-HISTORY.md; earlier claims/test
requests there describe their original versions, not a resolution of this crash.
