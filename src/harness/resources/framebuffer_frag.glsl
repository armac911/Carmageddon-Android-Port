in highp vec2 v_tex_coord;

uniform usampler2D u_pixels;       // current pixmap: CPU effects and 2D HUD
uniform usampler2D u_snapshot;     // pixmap immediately after the 3D readback
uniform usampler2D u_super_3d;     // bottom-up high-resolution palette indices
uniform sampler2D u_palette;
uniform int u_smooth;             // zero preserves the original nearest path
uniform highp vec2 u_output_size;  // size of the render target in pixels
uniform int u_mark_scene;          // 1: alpha = 3D-scene mask for post-processing
uniform int u_ui_scaler;           // menus/HUD: 0 nearest, 1 sharp (area), 2 smooth (Scale2x + area)
uniform highp float u_gamma;       // 1.0 = unchanged; >1 brightens mid-tones
uniform highp float u_saturation;  // 1.0 = unchanged
uniform highp float u_scanlines;   // 0.0 = off; darkening between game rows

layout (location = 0) out vec4 out_frag_color;

// Integer textures cannot use hardware linear filtering. Resolve the existing
// composite and its palette color BEFORE interpolating; never mix indices.
vec4 composite_color(highp vec2 uv) {
  uint ix_now = texture(u_pixels, uv).r;
  uint ix_snap = texture(u_snapshot, uv).r;
  uint ix_super = texture(u_super_3d, vec2(uv.x, 1.0 - uv.y)).r;
  uint chosen = (ix_now != ix_snap) ? ix_now : (ix_super != 0u ? ix_super : ix_now);
  return texelFetch(u_palette, ivec2(chosen, 0), 0).bgra;
}

vec4 color_at(highp vec2 pixel, highp vec2 size) {
  // Clamp centers explicitly so filtering cannot wrap across screen edges.
  highp vec2 uv = (clamp(pixel, vec2(0.0), size - 1.0) + 0.5) / size;
  return composite_color(uv);
}

// Supersampling resolve. Each output pixel covers a rectangle of the
// super-resolution grid; average every super texel it overlaps, weighted by
// the overlapped area. When one output pixel is smaller than one texel this
// degenerates to the original nearest lookup, so Performance quality and the
// upscaler source pass are unchanged. Colors are averaged in approximately
// linear light (gamma 2) so edges do not darken.
const int SSAA_MAX_TAPS = 4; // per axis; covers up to 3x downscale exactly

vec4 resolve_supersampled(highp vec2 uv) {
  highp vec2 grid = vec2(textureSize(u_super_3d, 0));
  highp vec2 half_footprint = 0.5 / u_output_size;
  highp vec2 p0 = clamp(uv - half_footprint, 0.0, 1.0) * grid;
  highp vec2 p1 = clamp(uv + half_footprint, 0.0, 1.0) * grid;
  highp vec2 t0 = floor(p0);
  // Footprints wider than SSAA_MAX_TAPS texels (tiny screens only) are
  // approximated by their leading texels; weights renormalise below.
  highp vec2 span = min(ceil(p1) - t0, vec2(float(SSAA_MAX_TAPS)));
  if (span.x <= 1.0 && span.y <= 1.0) return composite_color(uv);

  vec3 sum = vec3(0.0);
  float alpha = 0.0;
  highp float total = 0.0;
  for (int y = 0; y < SSAA_MAX_TAPS; ++y) {
    if (float(y) >= span.y) break;
    highp float ty = t0.y + float(y);
    highp float wy = min(p1.y, ty + 1.0) - max(p0.y, ty);
    for (int x = 0; x < SSAA_MAX_TAPS; ++x) {
      if (float(x) >= span.x) break;
      highp float tx = t0.x + float(x);
      highp float w = (min(p1.x, tx + 1.0) - max(p0.x, tx)) * wy;
      if (w <= 0.0) continue;
      vec4 c = composite_color((vec2(tx, ty) + 0.5) / grid);
      sum += c.rgb * c.rgb * w;
      alpha += c.a * w;
      total += w;
    }
  }
  if (total <= 0.0) return composite_color(uv);
  return vec4(sqrt(sum / total), alpha / total);
}

// ---- Menus, HUD and CPU-drawn pixels (the 640x480 pixmap layer) ----------

vec4 palette_color(uint index) {
  return texelFetch(u_palette, ivec2(index, 0), 0).bgra;
}

uint pixmap_index(ivec2 texel) {
  ivec2 limit = textureSize(u_pixels, 0) - 1;
  return texelFetch(u_pixels, clamp(texel, ivec2(0), limit), 0).r;
}

// Scale2x (EPX) on palette indices: index of sub-texel q (0/1 per axis) of
// pixmap texel s. Rounds stair-step diagonals in fonts and icons without
// inventing colours. Rules are symmetric, so texture-space orientation is fine.
uint scale2x_index(ivec2 s, ivec2 q) {
  uint e = pixmap_index(s);
  uint b = pixmap_index(s + ivec2(0, -1));
  uint h = pixmap_index(s + ivec2(0, 1));
  uint d = pixmap_index(s + ivec2(-1, 0));
  uint f = pixmap_index(s + ivec2(1, 0));
  if (b == h || d == f) return e;
  uint vertical = q.y == 0 ? b : h;
  uint horizontal = q.x == 0 ? d : f;
  return vertical == horizontal ? vertical : e;
}

uint ui_index(ivec2 t, int scale) {
  if (scale == 1) return pixmap_index(t);
  return scale2x_index(t / 2, t - (t / 2) * 2);
}

// Area-weighted sampling of the UI grid (pixmap x scale): every output pixel
// averages the grid cells it overlaps. Enlarging by a non-integer factor
// (e.g. 2.25x) then gives evenly sized pixels with one soft pixel at cell
// edges instead of nearest's uneven 2- and 3-pixel-wide cells.
vec4 ui_color(highp vec2 uv) {
  int scale = u_ui_scaler == 2 ? 2 : 1;
  highp vec2 grid = vec2(textureSize(u_pixels, 0) * scale);
  highp vec2 half_footprint = 0.5 / u_output_size;
  highp vec2 p0 = clamp(uv - half_footprint, 0.0, 1.0) * grid;
  highp vec2 p1 = clamp(uv + half_footprint, 0.0, 1.0) * grid;
  highp vec2 t0 = floor(p0);
  highp vec2 span = min(ceil(p1) - t0, vec2(float(SSAA_MAX_TAPS)));
  vec3 sum = vec3(0.0);
  highp float total = 0.0;
  for (int y = 0; y < SSAA_MAX_TAPS; ++y) {
    if (float(y) >= span.y) break;
    highp float ty = t0.y + float(y);
    highp float wy = min(p1.y, ty + 1.0) - max(p0.y, ty);
    for (int x = 0; x < SSAA_MAX_TAPS; ++x) {
      if (float(x) >= span.x) break;
      highp float tx = t0.x + float(x);
      highp float w = (min(p1.x, tx + 1.0) - max(p0.x, tx)) * wy;
      if (w <= 0.0) continue;
      vec3 c = palette_color(ui_index(ivec2(tx, ty), scale)).rgb;
      sum += c * c * w;
      total += w;
    }
  }
  if (total <= 0.0) return composite_color(uv);
  return vec4(sqrt(sum / total), 1.0);
}

// ---- Final colour adjustments ---------------------------------------------

vec3 adjust_color(vec3 c, highp vec2 uv) {
  // Unset uniforms read as 0: treat 0 as "unchanged" for gamma/saturation.
  if (u_saturation > 0.0 && u_saturation != 1.0) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = clamp(mix(vec3(l), c, u_saturation), 0.0, 1.0);
  }
  if (u_gamma > 0.0 && u_gamma != 1.0) c = pow(c, vec3(1.0 / u_gamma));
  if (u_scanlines > 0.0) {
    // Dark gaps between the game's original rows, bright row centres.
    highp float rows = float(textureSize(u_pixels, 0).y);
    highp float phase = fract(uv.y * rows);
    c *= mix(1.0 - u_scanlines, 1.0, sin(3.14159265 * phase));
  }
  return c;
}

#ifdef PRESENT_LEGACY_SMOOTH
vec4 legacy_smooth(highp vec2 uv) {
  // Keep the high-resolution 3D grid in 3D regions; use the original pixmap
  // grid for menus, HUD and CPU effects so those are smoothed as well.
  uint now = texture(u_pixels, uv).r;
  uint snap = texture(u_snapshot, uv).r;
  uint scene = texture(u_super_3d, vec2(uv.x, 1.0 - uv.y)).r;
  highp vec2 size = vec2(textureSize(u_pixels, 0));
  if (now == snap && scene != 0u) size = vec2(textureSize(u_super_3d, 0));
  highp vec2 point = uv * size - 0.5;
  highp vec2 base = floor(point);
  highp vec2 weight = fract(point);
  vec4 upper = mix(color_at(base, size), color_at(base + vec2(1.0, 0.0), size), weight.x);
  vec4 lower = mix(color_at(base + vec2(0.0, 1.0), size), color_at(base + vec2(1.0, 1.0), size), weight.x);
  return mix(upper, lower, weight.y);
}
#endif

// Features are compiled in only when a setting needs them (gl_renderer.c
// prepends the #defines), so the default build does exactly the 0.9.2 work:
//   PRESENT_SSAA          average super-res samples when shrinking the 3D image
//   PRESENT_UI_SCALER     Sharp/Smooth menu and HUD scaling
//   PRESENT_COLOR_ADJUST  gamma, saturation, scanlines
//   PRESENT_SCENE_MASK    3D mask in alpha for FXAA
//   PRESENT_LEGACY_SMOOTH old whole-image bilinear path (tests only)
void main(void) {
#ifdef PRESENT_LEGACY_SMOOTH
  if (u_smooth != 0) {
    out_frag_color = legacy_smooth(v_tex_coord);
    return;
  }
#endif
#if defined(PRESENT_UI_SCALER) || defined(PRESENT_SCENE_MASK)
  // Same test as composite_color: the 3D image shows where the CPU did not
  // draw over the post-readback snapshot and the scene covered the pixel.
  uint now = texture(u_pixels, v_tex_coord).r;
  uint snap = texture(u_snapshot, v_tex_coord).r;
  uint scene = texture(u_super_3d, vec2(v_tex_coord.x, 1.0 - v_tex_coord.y)).r;
  bool is_scene = now == snap && scene != 0u;
#endif
  bool done = false;
#ifdef PRESENT_UI_SCALER
  if (!is_scene && u_ui_scaler != 0) {
    out_frag_color = ui_color(v_tex_coord);
    done = true;
  }
#endif
  if (!done) {
#ifdef PRESENT_SSAA
    out_frag_color = resolve_supersampled(v_tex_coord);
#else
    out_frag_color = composite_color(v_tex_coord);
#endif
  }
#ifdef PRESENT_COLOR_ADJUST
  out_frag_color.rgb = adjust_color(out_frag_color.rgb, v_tex_coord);
#endif
#ifdef PRESENT_SCENE_MASK
  if (u_mark_scene != 0) out_frag_color.a = is_scene ? 1.0 : 0.0;
#endif
}
