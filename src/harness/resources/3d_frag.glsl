// Input, output variables
// =======================

in vec3 v_frag_pos;
in vec3 v_normal;
in vec2 v_tex_coord;

// this is actually an integer palette index. We define it as a float to allow opengl to
// interpolate the value across each face.
in float v_color;

// 256 color mode, so each calculated color is a palette index.
out uint out_palette_index;

// Uniform variables
// =================

// colour_buffer is an image which contains the current content of the framebuffer
uniform usampler2D u_colour_buffer;

uniform vec4 u_clip_planes[6];
uniform int u_clip_plane_count;

uniform uint u_material_flags;
uniform mat2x3 u_material_uv_transform;
uniform usampler2D u_material_texture_pixelmap;
uniform uint u_material_texture_enabled;

// material_blend_table is a 256x256 image which encodes 256 values of blending between new color and current color in framebuffer
uniform uint u_material_blend_enabled;
uniform usampler2D u_material_blend_table;

// material_shade_table is a 256px-wide image which encodes material_shade_table_height lit shades for each color
uniform usampler2D u_material_shade_table;

// how many lit shades are in the material_shade_table
uniform uint u_material_shade_table_height;

// For non-textured materials, defines the starting palette index
uniform uint u_material_index_base;
// For non-textured materials, defines the range of lit shades
uniform uint u_material_index_range;

// material_flags values
const uint BR_MATF_LIGHT = 1u;
const uint BR_MATF_PRELIT = 2u;

// Filtering works in texture space after the material's shade lookup.
// The framebuffer stays indexed so original CPU effects and blend tables work.
uniform int u_texture_filtering;
uniform sampler2D u_material_palette;
uniform highp usampler2D u_filter_lut;

uint shaded_index(uint index) {
    if ((u_material_flags & BR_MATF_LIGHT) != 0u) {
        uint level = (u_material_flags & BR_MATF_PRELIT) != 0u
            ? uint(v_color) / u_material_shade_table_height : 0u;
        return texelFetch(u_material_shade_table, ivec2(index, level), 0).r;
    }
    return index;
}

uint smooth_texture(highp vec2 uv, uint nearest_index) {
    highp vec2 size = vec2(textureSize(u_material_texture_pixelmap, 0));
    highp vec2 point = uv * size - 0.5;
    highp vec2 base = floor(point);
    highp vec2 f = fract(point);
    highp vec3 color = vec3(0.0);
    highp float total = 0.0;
    uint first = 0u;
    bool same = true;
    // Center coverage remains the original nearest texel. Exclude transparent
    // neighbors and renormalize to avoid dark fringes on cutout textures.
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            highp vec2 tap = (base + vec2(float(x), float(y)) + 0.5) / size;
            uint index = texture(u_material_texture_pixelmap, tap).r;
            highp float weight = (x == 0 ? 1.0 - f.x : f.x) * (y == 0 ? 1.0 - f.y : f.y);
            if (index != 0u && weight > 0.0) {
                uint lit = shaded_index(index);
                if (total == 0.0) first = lit; else if (lit != first) same = false;
                color += texelFetch(u_material_palette, ivec2(lit, 0), 0).rgb * weight;
                total += weight;
            }
        }
    }
    // Keep exact indices for uniform regions and duplicate palette colors.
    if (total <= 0.0) return nearest_index;
    if (same) return first;
    highp ivec3 q = ivec3(floor(clamp(color / total, 0.0, 1.0) * 31.0 + 0.5));
    highp int address = q.r + 32 * q.g + 1024 * q.b;
    return texelFetch(u_filter_lut, ivec2(address % 256, address / 256), 0).r;
}

void main(void) {

    for(int i = 0; i < u_clip_plane_count; i++) {
        // calculate signed plane-vertex distance
        vec4 v4 = vec4(v_frag_pos.xyz, 1);
        float d = dot(u_clip_planes[i], v4);
        if (d < 0.0) {
            discard;
        }
    }

    if (u_material_texture_enabled == 0u) {
        // force palette index, no texture lookup
        out_palette_index = u_material_index_base;

        if ((u_material_flags & BR_MATF_LIGHT) != 0u) {
            // TODO: lighting calculations based on https://rr2000.cwaboard.co.uk/R4/BRENDER/TEBK_43.HTM#0
        }
    }
    else {
        // calculate texture uv coordinates
        vec2 sample_coord = vec3(v_tex_coord.xy, 1) * u_material_uv_transform;
        uint texel = texture(u_material_texture_pixelmap, sample_coord.xy).r;

        // color 0 is always transparent
        if (texel == 0u) {
            discard;
        }

        out_palette_index = shaded_index(texel);
        if (u_texture_filtering != 0) {
            out_palette_index = smooth_texture(sample_coord, out_palette_index);
        }
    }

    if (u_material_blend_enabled == 1u) {
        // The snapshot is copied GPU-to-GPU from this same framebuffer.
        // Both use bottom-left pixel coordinates. Flipping Y reads unrelated
        // rows and the old height-y expression fetched past the top edge.
        highp ivec2 coords = ivec2(gl_FragCoord.xy);
        uint current_framebuffer_color = texelFetch(u_colour_buffer, coords, 0).r;

        out_palette_index = texelFetch(u_material_blend_table, ivec2(out_palette_index, current_framebuffer_color), 0).r;
    }

    // HACK: Pick another black color instead of palette index 0 so we can detect which pixels have been drawn this frame in FlushBuffers
    if (out_palette_index == 0u) {
        out_palette_index = 240u;
    }
}
