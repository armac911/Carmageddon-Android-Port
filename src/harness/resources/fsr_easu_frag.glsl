// AMD FidelityFX Super Resolution 1.0 — EASU (Edge Adaptive Spatial Upsampling).
// GLSL ES 3.0 port of FsrEasuF from ffx_fsr1.h (fp32 path).
// Copyright (c) 2021 Advanced Micro Devices, Inc. MIT License; see
// THIRD-PARTY-NOTICES.md. GLES 3.0 has no textureGather, so the 12 taps are
// fetched individually with texelFetch, clamped to the source edges.

uniform sampler2D u_source;       // RGBA8 composited frame, perceptual (gamma) space
uniform highp vec2 u_source_size; // source size in pixels
uniform highp vec2 u_output_size; // EASU target size in pixels

layout (location = 0) out vec4 out_frag_color;

vec3 easu_tap(ivec2 p) {
  ivec2 limit = ivec2(u_source_size) - 1;
  return texelFetch(u_source, clamp(p, ivec2(0), limit), 0).rgb;
}

float easu_luma(vec3 c) {
  return c.b * 0.5 + (c.r * 0.5 + c.g);
}

// Accumulate direction and edge length for one of the four bilinear corners.
// a = above, b = left, c = centre, d = right, e = below.
void easu_set(inout vec2 dir, inout float len, float w,
              float la, float lb, float lc, float ld, float le) {
  float dc = ld - lc;
  float cb = lc - lb;
  float len_x = 1.0 / max(max(abs(dc), abs(cb)), 1.0e-5);
  float dir_x = ld - lb;
  dir.x += dir_x * w;
  len_x = clamp(abs(dir_x) * len_x, 0.0, 1.0);
  len_x *= len_x;
  len += len_x * w;

  float ec = le - lc;
  float ca = lc - la;
  float len_y = 1.0 / max(max(abs(ec), abs(ca)), 1.0e-5);
  float dir_y = le - la;
  dir.y += dir_y * w;
  len_y = clamp(abs(dir_y) * len_y, 0.0, 1.0);
  len_y *= len_y;
  len += len_y * w;
}

// Approximate windowed-lanczos2 tap, rotated and stretched along the edge.
void easu_accumulate(inout vec3 acc, inout float acc_w, vec2 off, vec2 dir,
                     vec2 len2, float lob, float clp, vec3 c) {
  vec2 v = vec2(off.x * dir.x + off.y * dir.y,
                off.x * (-dir.y) + off.y * dir.x);
  v *= len2;
  float d2 = min(dot(v, v), clp);
  float wb = 0.4 * d2 - 1.0;
  float wa = lob * d2 - 1.0;
  wb *= wb;
  wa *= wa;
  wb = 1.5625 * wb - 0.5625;
  float w = wb * wa;
  acc += c * w;
  acc_w += w;
}

void main(void) {
  highp vec2 out_pixel = floor(gl_FragCoord.xy);
  highp vec2 scale = u_source_size / u_output_size;
  highp vec2 pp = (out_pixel + 0.5) * scale - 0.5;
  highp vec2 fp = floor(pp);
  pp -= fp;
  ivec2 o = ivec2(fp);

  //    b c
  //  e f g h
  //  i j k l
  //    n o
  vec3 b = easu_tap(o + ivec2( 0, -1));
  vec3 c = easu_tap(o + ivec2( 1, -1));
  vec3 e = easu_tap(o + ivec2(-1,  0));
  vec3 f = easu_tap(o + ivec2( 0,  0));
  vec3 g = easu_tap(o + ivec2( 1,  0));
  vec3 h = easu_tap(o + ivec2( 2,  0));
  vec3 i = easu_tap(o + ivec2(-1,  1));
  vec3 j = easu_tap(o + ivec2( 0,  1));
  vec3 k = easu_tap(o + ivec2( 1,  1));
  vec3 l = easu_tap(o + ivec2( 2,  1));
  vec3 n = easu_tap(o + ivec2( 0,  2));
  vec3 oo = easu_tap(o + ivec2( 1,  2));

  float bl = easu_luma(b), cl = easu_luma(c), el = easu_luma(e), fl = easu_luma(f);
  float gl = easu_luma(g), hl = easu_luma(h), il = easu_luma(i), jl = easu_luma(j);
  float kl = easu_luma(k), ll = easu_luma(l), nl = easu_luma(n), ol = easu_luma(oo);

  vec2 dir = vec2(0.0);
  float len = 0.0;
  easu_set(dir, len, (1.0 - pp.x) * (1.0 - pp.y), bl, el, fl, gl, jl);
  easu_set(dir, len, pp.x * (1.0 - pp.y),         cl, fl, gl, hl, kl);
  easu_set(dir, len, (1.0 - pp.x) * pp.y,         fl, il, jl, kl, nl);
  easu_set(dir, len, pp.x * pp.y,                 gl, jl, kl, ll, ol);

  // Normalise direction; fall back to +x for flat areas.
  vec2 dir2 = dir * dir;
  float dir_r = dir2.x + dir2.y;
  bool zero = dir_r < (1.0 / 32768.0);
  dir_r = zero ? 1.0 : inversesqrt(dir_r);
  dir.x = zero ? 1.0 : dir.x;
  dir *= dir_r;

  // Shape the filter kernel: stretch along edges, sharpen across them.
  len = len * 0.5;
  len *= len;
  float stretch = dot(dir, dir) / max(abs(dir.x), abs(dir.y));
  vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
  float lob = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * len;
  float clp = 1.0 / lob;

  // Ringing removal bounds from the nearest 2x2.
  vec3 min4 = min(min(f, g), min(j, k));
  vec3 max4 = max(max(f, g), max(j, k));

  vec3 acc = vec3(0.0);
  float acc_w = 0.0;
  easu_accumulate(acc, acc_w, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, b);
  easu_accumulate(acc, acc_w, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, c);
  easu_accumulate(acc, acc_w, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, i);
  easu_accumulate(acc, acc_w, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, j);
  easu_accumulate(acc, acc_w, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, f);
  easu_accumulate(acc, acc_w, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, e);
  easu_accumulate(acc, acc_w, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, k);
  easu_accumulate(acc, acc_w, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, l);
  easu_accumulate(acc, acc_w, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, h);
  easu_accumulate(acc, acc_w, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, g);
  easu_accumulate(acc, acc_w, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, oo);
  easu_accumulate(acc, acc_w, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, n);

  vec3 color = min(max4, max(min4, acc / acc_w));
  out_frag_color = vec4(color, 1.0);
}
