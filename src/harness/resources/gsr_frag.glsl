// Snapdragon Game Super Resolution 1 (SGSR1) — single-pass upscale + sharpen.
// Port of sgsr1_shader_mobile_edge_direction.frag (RGBA mode, edge direction
// enabled) from https://github.com/SnapdragonStudios/snapdragon-gsr.
// Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause; see THIRD-PARTY-NOTICES.md.
//
// GLES 3.0 has no textureGather, so each gather of the green channel is
// replaced by four texelFetch calls on exactly the texels the gather reads.

uniform sampler2D u_source;        // RGBA8 composited frame, GL_LINEAR filtered
uniform highp vec2 u_source_size;  // source size in pixels
uniform highp vec2 u_output_size;  // viewport size in pixels
uniform highp vec2 u_offset;       // window-space origin of the viewport

layout (location = 0) out vec4 out_frag_color;

#define EDGE_THRESHOLD (8.0 / 255.0)
#define EDGE_SHARPNESS 2.0

float green_at(ivec2 p) {
  ivec2 limit = ivec2(u_source_size) - 1;
  return texelFetch(u_source, clamp(p, ivec2(0), limit), 0).g;
}

// Same component order as textureGather at the texel corner p:
// x = (p.x-1, p.y), y = (p.x, p.y), z = (p.x, p.y-1), w = (p.x-1, p.y-1).
vec4 gather_green(ivec2 p) {
  return vec4(green_at(p + ivec2(-1, 0)), green_at(p),
              green_at(p + ivec2(0, -1)), green_at(p + ivec2(-1, -1)));
}

float fast_lanczos2(float x) {
  float wa = x - 4.0;
  float wb = x * wa - wa;
  wa *= wa;
  return wb * wa;
}

vec2 weight_y(float dx, float dy, float c, vec3 data) {
  float std_dev = data.x;
  vec2 dir = data.yz;
  float edge_dis = dx * dir.y + dy * dir.x;
  float x = (dx * dx + dy * dy) + (edge_dis * edge_dis) * (clamp(c * c * std_dev, 0.0, 1.0) * 0.7 - 1.0);
  float w = fast_lanczos2(x);
  return vec2(w, w * c);
}

vec2 edge_direction(vec4 left, vec4 right) {
  float rx_lz = right.x - left.z;
  float rw_ly = right.w - left.y;
  vec2 delta = vec2(rx_lz + rw_ly, rx_lz - rw_ly);
  float length_inv = inversesqrt(delta.x * delta.x + 3.075740e-05 + delta.y * delta.y);
  return delta * length_inv;
}

void main(void) {
  highp vec2 uv = (gl_FragCoord.xy - u_offset) / u_output_size;
  vec4 color = vec4(textureLod(u_source, uv, 0.0).rgb, 0.0);

  highp vec2 img_coord = uv * u_source_size + vec2(-0.5, 0.5);
  highp vec2 img_coord_pixel = floor(img_coord);
  ivec2 p = ivec2(img_coord_pixel);
  vec2 pl = img_coord - img_coord_pixel;
  vec4 left = gather_green(p);

  float edge_vote = abs(left.z - left.y) + abs(color.g - left.y) + abs(color.g - left.z);
  if (edge_vote > EDGE_THRESHOLD) {
    vec4 right = gather_green(p + ivec2(2, 0));
    vec4 up_down;
    up_down.xy = gather_green(p + ivec2(1, -1)).wz;
    up_down.zw = gather_green(p + ivec2(1, 1)).yx;

    float mean = (left.y + left.z + right.x + right.w) * 0.25;
    left -= vec4(mean);
    right -= vec4(mean);
    up_down -= vec4(mean);
    color.w = color.g - mean;

    float sum = dot(abs(left), vec4(1.0)) + dot(abs(right), vec4(1.0)) + dot(abs(up_down), vec4(1.0));
    float sum_mean = 1.014185e+01 / max(sum, 1.0e-5);
    float std_dev = sum_mean * sum_mean;
    vec3 data = vec3(std_dev, edge_direction(left, right));

    vec2 awy = weight_y(pl.x, pl.y + 1.0, up_down.x, data);
    awy += weight_y(pl.x - 1.0, pl.y + 1.0, up_down.y, data);
    awy += weight_y(pl.x - 1.0, pl.y - 2.0, up_down.z, data);
    awy += weight_y(pl.x, pl.y - 2.0, up_down.w, data);
    awy += weight_y(pl.x + 1.0, pl.y - 1.0, left.x, data);
    awy += weight_y(pl.x, pl.y - 1.0, left.y, data);
    awy += weight_y(pl.x, pl.y, left.z, data);
    awy += weight_y(pl.x + 1.0, pl.y, left.w, data);
    awy += weight_y(pl.x - 1.0, pl.y - 1.0, right.x, data);
    awy += weight_y(pl.x - 2.0, pl.y - 1.0, right.y, data);
    awy += weight_y(pl.x - 2.0, pl.y, right.z, data);
    awy += weight_y(pl.x - 1.0, pl.y, right.w, data);

    float final_y = awy.y / awy.x;
    float max_y = max(max(left.y, left.z), max(right.x, right.w));
    float min_y = min(min(left.y, left.z), min(right.x, right.w));
    float delta_y = clamp(EDGE_SHARPNESS * final_y, min_y, max_y) - color.w;
    // Smooth high-contrast input.
    delta_y = clamp(delta_y, -23.0 / 255.0, 23.0 / 255.0);
    color.rgb = clamp(color.rgb + delta_y, 0.0, 1.0);
  }

  out_frag_color = vec4(color.rgb, 1.0);
}
