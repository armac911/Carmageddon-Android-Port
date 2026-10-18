# Contributing

This Android fork is maintained as an AndroGaming.com project. Report problems
in this repository rather than assuming an Android-specific change is upstream.
Search existing issues first. Include app version, device/Android version,
controller model, graphics settings, steps, expected result and observed result.
A screenshot or optional diagnostics export can help; never attach game data,
private signing material or personal files.

For code changes, explain the player-visible problem and the resulting behavior,
keep unrelated changes separate, and run relevant existing tests. Preserve source
notices, use the applicable existing licenses and credit prior work. Disclose
substantial AI assistance and describe how the result was checked. Do not add
original game assets or private credentials. Follow upstream C style where
applicable; keep Android/platform changes scoped to their platform.

The maintainer's APK signing key is private. Local/CI debug keys produce separate
signing identities; do not replace published APKs with those builds as updates.
No automated public-release process is configured.

The original Dethrace contribution guidance is retained in
[docs/CONTRIBUTING-upstream.md](docs/CONTRIBUTING-upstream.md). If a fix also belongs
upstream, discuss it with the appropriate maintainers and distinguish it from
this fork's Android features.
