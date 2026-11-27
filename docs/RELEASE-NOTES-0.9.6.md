# 0.9.6 native cockpit composition test

The 0.9.5 fill replaced stretched side textures with black rectangular blocks.
This build removes the artificial cockpit side extension entirely. It keeps
the original artwork centred at native size and leaves the rendered world
visible outside its bounds and through transparent windows. Extra cockpit
interior detail is not invented or stretched to fill a wider display.

Control tuning, partial triggers and other PR #5 fixes remain unchanged.
Install over the previous maintainer build without uninstalling. Compare the
same cockpit/device-fit view on S24 FE and Flip 2, including turns and impacts.
Synthetic production-copier tests verify original pixels, transparent windows,
untouched world at both sides, camera wobble, clipping and guard bytes. Device
confirmation remains pending. The existing cockpit FPS limitation remains.

Cianid retains credit for the original PR #5 fixes. See README for AI disclosure.
