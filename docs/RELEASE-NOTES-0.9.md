# Carmageddon Android Port 0.9

Adds launcher selection and isolated import/save roots for full Carmageddon
Splat Pack, using the inherited Dethrace expansion engine path. No expansion
content is included. Existing Carmageddon data/saves stay in place.

Install over 0.8 or the earlier 1.0 candidate without uninstalling. On Home,
choose Splat Pack and import your own complete expansion DATA/ ZIP (optional
MUSIC/). In a Max Pack installation, start from CARSPLAT/DATA/ and merge matching
support DATA files if supplied; DATA/ must be directly at the ZIP root.
Exit the game before switching editions.

Includes a fix for the nonfatal USB receiver-registration error found in the
Android 16 log. Structural import/save-preservation tests, production native
edition detection and API-version receiver tests pass. Physical Splat Pack
intro/race/save-load testing with the maintainer's assets is still required.
The earlier intermittent exit is not declared fixed; keep error capture enabled
for the first expansion test if useful. Both diagnostic recorders default off.

The submitted 0.8 session records a normal exit/status 0 with no crash evidence.
Its captured early racing interval averages 57.9 FPS at High/Far on S24 FE.
See README, CHANGELOG and PERFORMANCE-NOTES for requirements, AI disclosure,
credits and measurement limits.
