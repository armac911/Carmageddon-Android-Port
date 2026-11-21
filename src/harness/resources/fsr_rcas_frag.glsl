// AMD FidelityFX Super Resolution 1.0 — RCAS (Robust Contrast Adaptive Sharpening).
// GLSL ES 3.0 port of FsrRcasF from ffx_fsr1.h (fp32 path, no denoise).
// Copyright (c) 2021 Advanced Micro Devices, Inc. MIT License; see
// THIRD-PARTY-NOTICES.md.

uniform sampler2D u_source;        // EASU output, same size as the viewport
uniform highp vec2 u_source_size;  // EASU output size in pixels
uniform highp vec2 u_offset;       // window-space origin of the viewport
uniform float u_sharpness;         // exp2(-stops); 1.0 = maximum sharpening

layout (location = 0) out vec4 out_frag_color;

// Lobe limit that keeps the sharpening filter from going unstable.
#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))

vec3 rcas_tap(ivec2 p) {
  ivec2 limit = ivec2(u_source_size) - 1;
  return texelFetch(u_source, clamp(p, ivec2(0), limit), 0).rgb;
}

void main(void) {
  ivec2 sp = ivec2(floor(gl_FragCoord.xy - u_offset));

  //    b
  //  d e f
  //    h
  vec3 b = rcas_tap(sp + ivec2( 0, -1));
  vec3 d = rcas_tap(sp + ivec2(-1,  0));
  vec3 e = rcas_tap(sp);
  vec3 f = rcas_tap(sp + ivec2( 1,  0));
  vec3 h = rcas_tap(sp + ivec2( 0,  1));

  vec3 mn4 = min(min(b, d), min(f, h));
  vec3 mx4 = max(max(b, d), max(f, h));

  // Largest negative lobe that keeps the result within [0, 1] for each channel.
  vec3 hit_min = min(mn4, e) / max(4.0 * mx4, vec3(1.0e-5));
  vec3 hit_max = (1.0 - max(mx4, e)) / min(4.0 * mn4 - 4.0, vec3(-1.0e-5));
  vec3 lobe3 = max(-hit_min, hit_max);
  float lobe = max(-FSR_RCAS_LIMIT, min(max(lobe3.r, max(lobe3.g, lobe3.b)), 0.0)) * u_sharpness;

  float rcp_l = 1.0 / (4.0 * lobe + 1.0);
  vec3 color = (lobe * (b + d + h + f) + e) * rcp_l;
  out_frag_color = vec4(clamp(color, 0.0, 1.0), 1.0);
}
