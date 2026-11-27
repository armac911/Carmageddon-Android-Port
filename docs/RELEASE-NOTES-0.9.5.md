# 0.9.5 cockpit edge test

Follow-up to the combined 0.9.4 candidate. Contains the same control tuning,
proportional pedals and PR #5 integration. The maintainer confirmed working
partial triggers and other controls on Retroid Flip 2; horizontal cockpit side
streaking also appears on S24 FE and Flip 2.

This update retains each cockpit edge's opaque/transparent silhouette but fills
the extra opaque area with palette black instead of repeating textured edge
colours into horizontal lines. Original cockpit artwork is preserved. Transparent
window rows still show the world. No physics, controller or mirror changes.

Install over the previous maintainer build without uninstalling. On both phones,
compare device-fit and 16:9 cockpit views, including a turn and an impact/camera
wobble. Check the left and right sides and visibility through windows. The
synthetic rendering regression covers opacity, absence of coloured streaks,
clipping and untouched window/artwork areas. Physical visual confirmation is
pending. The existing cockpit FPS limitation remains.

See the 0.9.4 notes and README for Cianid's contribution and AI disclosure.
