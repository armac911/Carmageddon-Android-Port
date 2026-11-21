// Fast approximate anti-aliasing (FXAA-style, single pass) for the composited
// RGBA frame. Finds the local edge direction from the luma of the four
// diagonal neighbours and blends along it; low-contrast texels are left
// untouched so texture detail survives.
//
// Alpha carries a 3D-scene mask written by the present shader: menus, HUD and
// CPU-drawn effects (alpha 0) pass through unchanged so their pixel art and
// text stay crisp.

uniform sampler2D u_source;        // RGBA8, GL_LINEAR filtered
uniform highp vec2 u_source_size;  // source size in pixels (== output size)
uniform highp vec2 u_offset;       // window-space origin of the output

layout (location = 0) out vec4 out_frag_color;

// Ignore edges whose contrast is below max(EDGE_MIN, EDGE_REL * local max).
const float EDGE_MIN = 1.0 / 24.0;
const float EDGE_REL = 1.0 / 8.0;
// Direction estimation (as in the classic FXAA "lite" variant).
const float REDUCE_MIN = 1.0 / 128.0;
const float REDUCE_MUL = 1.0 / 8.0;
const float SPAN_MAX = 8.0;

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main(void) {
  highp vec2 texel = 1.0 / u_source_size;
  highp vec2 uv = (gl_FragCoord.xy - u_offset) * texel;
  vec4 centre = texture(u_source, uv);
  if (centre.a < 0.5) {
    out_frag_color = vec4(centre.rgb, 1.0);
    return;
  }

  float l_nw = luma(texture(u_source, uv + vec2(-1.0, -1.0) * texel).rgb);
  float l_ne = luma(texture(u_source, uv + vec2( 1.0, -1.0) * texel).rgb);
  float l_sw = luma(texture(u_source, uv + vec2(-1.0,  1.0) * texel).rgb);
  float l_se = luma(texture(u_source, uv + vec2( 1.0,  1.0) * texel).rgb);
  float l_m = luma(centre.rgb);
  float l_min = min(l_m, min(min(l_nw, l_ne), min(l_sw, l_se)));
  float l_max = max(l_m, max(max(l_nw, l_ne), max(l_sw, l_se)));
  if (l_max - l_min < max(EDGE_MIN, l_max * EDGE_REL)) {
    out_frag_color = vec4(centre.rgb, 1.0);
    return;
  }

  // The gradient points across the edge; blur along the perpendicular.
  highp vec2 dir = vec2(-((l_nw + l_ne) - (l_sw + l_se)), (l_nw + l_sw) - (l_ne + l_se));
  float reduce = max((l_nw + l_ne + l_sw + l_se) * 0.25 * REDUCE_MUL, REDUCE_MIN);
  float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
  dir = clamp(dir * scale, vec2(-SPAN_MAX), vec2(SPAN_MAX)) * texel;

  // Two taps close to the centre, then two at the ends of the span.
  vec3 near = 0.5 * (texture(u_source, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                     texture(u_source, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
  vec3 wide = near * 0.5 + 0.25 * (texture(u_source, uv - dir * 0.5).rgb +
                                   texture(u_source, uv + dir * 0.5).rgb);
  // If the wide span crossed into another edge, fall back to the near blend.
  float l_wide = luma(wide);
  out_frag_color = vec4((l_wide < l_min || l_wide > l_max) ? near : wide, 1.0);
}
