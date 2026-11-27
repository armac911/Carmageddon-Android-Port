# 0.9.7 map layout test

The map retained its original 640x480 coordinates inside the wide game canvas,
leaving the previous race image visible to its right. This build composes the
whole legacy map screen as one image, fits it at full height, centres it, and
clears the side bars. Proportions are preserved as requested by the maintainer.
The map bitmap, checkpoints, car markers, timer and live preview scale together.
CPU presentation is forced for the entire map so matching palette indices cannot
reveal old GPU geometry through the map or its black side bars.

Install over the previous maintainer build without uninstalling. On S24 FE and
Flip 2, toggle the map from both cockpit/external views in 4:3, 16:9 and device-fit
modes. Check that the map is centred, the side bars stay black and the preview,
checkpoint highlights and player blip align. Close/reopen it while moving.

The maintainer confirmed the 0.9.6 cockpit fix. Controls are unchanged.
The rear-view mirror remains cockpit-only as in the inherited engine; the map's
small live driving preview is a separate feature. No external mirror is added.
Device confirmation of this map change is pending. See README for credits/AI.
