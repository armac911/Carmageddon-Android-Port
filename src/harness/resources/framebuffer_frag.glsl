in highp vec2 v_tex_coord;

uniform usampler2D u_pixels;       // current pixmap: CPU effects and 2D HUD
uniform usampler2D u_snapshot;     // pixmap immediately after the 3D readback
uniform usampler2D u_super_3d;     // bottom-up high-resolution palette indices
uniform sampler2D u_palette;
uniform int u_smooth;             // zero preserves the original nearest path

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

void main(void) {
  if (u_smooth == 0) {
    out_frag_color = composite_color(v_tex_coord);
    return;
  }

  // Keep the high-resolution 3D grid in 3D regions; use the original pixmap
  // grid for menus, HUD and CPU effects so those are smoothed as well.
  uint now = texture(u_pixels, v_tex_coord).r;
  uint snap = texture(u_snapshot, v_tex_coord).r;
  uint scene = texture(u_super_3d, vec2(v_tex_coord.x, 1.0 - v_tex_coord.y)).r;
  highp vec2 size = vec2(textureSize(u_pixels, 0));
  if (now == snap && scene != 0u) size = vec2(textureSize(u_super_3d, 0));
  highp vec2 point = v_tex_coord * size - 0.5;
  highp vec2 base = floor(point);
  highp vec2 weight = fract(point);
  vec4 upper = mix(color_at(base, size), color_at(base + vec2(1.0, 0.0), size), weight.x);
  vec4 lower = mix(color_at(base + vec2(0.0, 1.0), size), color_at(base + vec2(1.0, 1.0), size), weight.x);
  out_frag_color = mix(upper, lower, weight.y);
}
