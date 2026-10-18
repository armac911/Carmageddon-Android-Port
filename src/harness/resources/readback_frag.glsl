// Exact replacement for the old CPU nearest-neighbour downsample.
// Integer coordinates must remain high precision at 3x widescreen resolution.
uniform highp usampler2D source_pixels;
uniform highp int sample_factor;
uniform highp usampler2D shadow_pixels;
uniform bool use_shadow_pixels;
layout(location = 0) out highp uint palette_index;
void main() {
    highp ivec2 pixel = ivec2(gl_FragCoord.xy) * sample_factor;
    palette_index = texelFetch(source_pixels, pixel, 0).r;
    if (use_shadow_pixels && palette_index == 0u)
        palette_index = texelFetch(shadow_pixels, pixel, 0).r;
}
