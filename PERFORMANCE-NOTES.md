# Far-distance performance investigation — 0.1.11

## Evidence in this fork

- `src/BRSRC13/CORE/V1DB/render.c: BrOnScreenCheck` is an unconditional stub
  returning 1. The renderer therefore receives models outside the view.
- `common/brucetrk.c: RenderTrack` selects columns using a rectangle enclosing
  view corners. That broad selection is not an exact frustum visibility test.
- `GLRenderer_Model` previously calculated a view-model inverse/transpose for
  every model. `3d_vert.glsl` only uses it for spherical environment mapping.

## Implemented

Conservative bounds tests using the actual shader matrices and actual uploaded
vertices. Reject only when all of a model's bounding volume lies outside one
of six clip planes. Use a rounding margin; retain uncertain/invalid cases.
Update bounds whenever the model's GPU mesh is rebuilt. Keep environment-map
normal calculations; omit them for ordinary materials. This avoids draw calls
and vertex/material processing without shortening Far distance.

## Research

- Arm GLES terrain sample and bounding-box frustum test:
  https://arm-software.github.io/opengl-es-sdk-for-android/terrain.html
  https://arm-software.github.io/opengl-es-sdk-for-android/_frustum_8cpp_source.html
- Android CPU/GPU optimization guidance emphasizes identifying bottlenecks and
  reducing geometry/draw overhead through culling and frame inspection:
  https://developer.android.com/games/optimize/optimization-tips
- Reviewed upstream Dethrace performance issue #457; its visible body asks
  about draw distance and performance but provides no measured fix to copy:
  https://github.com/dethrace-labs/dethrace/issues/457

## Validation and limits

40,000 randomized boxes compare the production test with independent eight-
corner model/view/projection transforms. Both aspect ratios, two far planes,
rotations, translations, shear, scale and reflection are covered; visible
reference cases are never rejected. Near-plane and edge cases plus invalid
coordinates fail conservatively. Existing GLES readback tests still pass.
These are correctness tests, not phone benchmarks. No device/game assets are
available here. Objects hidden behind buildings are not yet occlusion-culled;
scene traversal and simulation work remain. If Far is still costly, next use
on-device CPU/GPU profiling to distinguish draw submission, visible geometry,
readback stalls and simulation before larger batching/occlusion changes.

## 0.1.12 profiling follow-up

Owner confirms ~30 FPS at race start at maximum settings after 0.1.11 culling.
High -> Performance while retaining Far adds approximately 7 FPS. This motivates
measurement rather than another quality reduction. See START-HERE.md for capture
and timing interpretation. New instrumentation keeps existing rendering behavior.

## Owner capture and 0.1.13

Capture carma-profiling-20261018-141212.zip contains High/Far and Performance/Far
runs from SM-S721B Android 16, Samsung ANGLE/Xclipse 940. Seconds 5–10:
High: frame 32.206 ms, readback 16.655 ms, copy 5.514 ms, 6.535 readbacks/frame.
Performance: frame 23.536 ms, readback 10.795 ms, copy 3.991 ms, 4.689 reads/frame.
Scene workloads differ; do not treat this as controlled resolution-only scaling.
Battery readings 30.5–31.7 C, reported thermal state 0, unplugged, power save off.
Render/model/readback fields overlap and must not be summed as disjoint stages.

Deeper source tracing finds ProcessShadow ends a BRender scene per car, whose
BrZbSceneRenderEnd calls FlushBuffers. Batch only that GPU-only region and flush
once before the main scene. Keep oil-intersecting shadows separate. Indexed blend
snapshot copying stays on GPU; remove its unnecessary readback/colour clear.
Remaining CPU effect boundaries retain their normal synchronous transfers.

## 0.1.14

0.1.13 owner validation: effects look correct. New High/Far capture seconds 5–10:
18.976 ms frame, 9.500 ms readback, 1.576 ms CPU copy, exactly 2 reads/frame.
Defer the last shadow batch into a separate GPU texture, merge palette coverage
in the canvas downsampling shader at the next true CPU boundary. Main-scene blend
snapshots remain separate, avoiding a change in their background palette inputs.
No GPU-to-CPU transfer is introduced for the deferred layer. Exact synthetic
comparison against two separate readbacks passes. Hardware gains unmeasured.


## 0.1.19 GPU compatibility investigation

Owner reports 0.1.18 works visually on S24 FE, while Flip 2 video contains shadow
instability and colored road streaks. The available video alone does not establish
one root cause or a specific GPU driver defect.

A production-shader test with an 8x8 spatially varying background reproduces
56/64 wrong indexed blend results: a GPU-to-GPU copied texture was sampled
with a Y inversion and an off-by-one height-y coordinate. Direct gl_FragCoord
pixel coordinates produce the expected result. This is a verified coordinate
bug, independent of the unconfirmed device-specific cause.

The GLES header also forced mediump for geometry and clipping. ES permits lower
precision there; implementations can use more precision, so different GPUs need
not exhibit identical errors. This release requests highp and uses an Android
24-bit depth attachment. Numerical tests exercise sub-unit world/camera clipping
and separation of two nearby surfaces. Hardware confirmation remains necessary.

References:
- https://registry.khronos.org/OpenGL/specs/es/3.0/GLSL_ES_Specification_3.00.pdf
  (precision requirements, section 4.5)
- https://registry.khronos.org/OpenGL/specs/es/3.0/es_spec_3.0.pdf
  (framebuffer, fragment/depth and texture coordinate conventions)


## 0.2 milestone review of Flip 2 capture

Input: carma-profiling-20261018-1941ip58.zip, app 0.1.19-dev, Android 13,
Qualcomm Adreno 650 native GLES 3.2 driver V@0746.0. Settings: 16:9, High, Far,
Sharp; the old metadata string "default" for FPS/steering resolves to 60/analog.
Owner confirms the shadow fix. Excluding interrupted frames and session startup
before five seconds leaves 4,920 frames:

| Metric | Mean |
| --- | --- |
| Effective FPS (1000 / mean frame ms) | 49.44 |
| Frame | 20.23 ms |
| Update | 0.70 ms |
| Render (inclusive) | 19.52 ms |
| GPU readback / synchronization | 8.74 ms |
| CPU copy | 1.43 ms |
| Model submission (nested) | 2.43 ms |
| Presentation | 0.34 ms |
| Swap | 0.69 ms |
| Limiter | 0.45 ms |
| Readbacks/frame | 1.44 |
| Draw calls/frame | 608.59 |

Median frame time is 19.24 ms; p95 is 26.00 ms. These nested wall-clock timings
must not be summed. Readback includes queued GPU work, not just byte transfer.

Seconds 5–20 average 56.3 FPS, ~582 draws and 1.08 readbacks/frame. From second
90 onward the remaining captured portion averages 40.0 FPS, ~834 draws and
2.00 readbacks/frame. Scene/workload changes provide a concrete explanation for
part of the slowdown. Battery readings range 28.0–29.7 C, and thermal status
remains 0; these are not CPU/GPU temperature or clock measurements.

No new rendering performance claim is made for 0.2. Readback synchronization is
the largest measured remaining target; reducing CPU simulation work is unlikely
to address this capture's primary cost. Further optimization needs controlled
scenes and preservation of CPU effects/readback boundaries. The milestone also adds controller input and clearer default-setting metadata.


### Revised 0.2: masked readback copy

The Android readback already produces contiguous canvas-resolution rows, but
FlushBuffer previously copied each nonzero pixel with a scalar branch and
per-pixel address arithmetic. The revised path merges 16 pixels at a time on
ARM64 using NEON compares/selects. Palette index zero leaves the existing CPU
pixel intact; row stride, sub-pixmap offsets and Y orientation are unchanged.
The scalar fallback uses the same exact merge rule. Non-canvas readback keeps
the previous supersample path.

This targets the measured 1.43 ms CPU-copy category without changing GPU
synchronization, shadow boundaries or palette output. Headless production GPU
flush tests preserve exact output at 1x/2x/3x, including shadows and blends.
Device FPS and copy-time improvement have not yet been measured. The 8.74 ms
GPU synchronization category remains the larger unresolved cost.
