#include "gl_renderer.h"
#include "palette_filter.h"
#include "texture_mips.h"
#include "indexed_copy.h"
#include "harness/profiler.h"
#include "harness/render_batch.h"
#include "brender/brender.h"
#include "embedded_resources.h"
#include "harness.h"
#include "harness/trace.h"
#include "stored_context.h"
#include "frustum.h"
#include <float.h>

#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tOpenGL_profile opengl_profile;
static GLuint screen_buffer_vao, screen_buffer_ebo;
static GLuint fullscreen_quad_texture, palette_texture, depth_texture;
static GLuint filter_lut_texture;
static int texture_filtering, filter_lut_ready;
/* Palette the filter LUT was built for, and the current palette's scale
 * relative to it (see DRPaletteFadeScale). */
static uint8_t filter_lut_palette[4 * 256];
static GLint filter_palette_scale_location;

static GLuint shader_program_2d;
static GLuint shader_program_3d;
#ifdef __ANDROID__
static GLuint readback_program, readback_framebuffer, readback_texture;
static GLint readback_sample_factor, readback_shadow_pixels, readback_use_shadow;
static GLuint deferred_shadow_texture;
static int pending_shadow;
#endif
static GLuint framebuffer_id, framebuffer_texture = 0;
#ifdef __ANDROID__
static GLuint mirror_fbo,mirror_colour,mirror_depth;
static int mirror_scene;
/* Mirror render scale: 1 by default (it is shown at canvas resolution);
 * DETHRACE_MIRROR_RES=full restores 0.9.2's full 3D resolution. */
static int mirror_factor = 1;
static int cpu_rect[4];
static void SwapMirrorTarget(void) {
    GLuint swap=framebuffer_id;framebuffer_id=mirror_fbo;mirror_fbo=swap;
    swap=framebuffer_texture;framebuffer_texture=mirror_colour;mirror_colour=swap;
    swap=depth_texture;depth_texture=mirror_depth;mirror_depth=swap;
}
void DRPresentCpuRect(int x,int y,int width,int height) {
    cpu_rect[0]=x;cpu_rect[1]=y;cpu_rect[2]=width;cpu_rect[3]=height;
}
#endif

// Pixmap snapshot taken right after the 3D readback inside FlushBuffer; the
// present-time shader compares the current pixmap to this snapshot to detect
// post-3D 2D draws (HUD/menu) so the rest can be drawn from the super-res
// 3D framebuffer instead of the downsampled pixmap.
static GLuint snapshot_texture = 0;
static uint8_t* snapshot_pixels = NULL;
/* Snapshot upload state: skip re-uploading an unchanged (e.g. all-zero menu)
 * snapshot every frame. */
static int snapshot_dirty = 1, snapshot_nonzero;
/* Current 3D scene viewport in framebuffer pixels (set by BeginScene). */
static int scene_viewport[4];
/* Filter LUT scale for palette fades, applied to the 3D program at BeginScene. */
static float filter_palette_scale = 1.0f;

// holds the latest uploaded version of the colour_buffer. Available in shader for blending
static GLuint current_colourbuffer_texture;

static uint8_t gl_palette[4 * 256]; // RGBA
/* Last palette bright enough to rank colours when building texture mips. */
static uint8_t mip_palette[4 * 256];
static int mip_palette_valid;
/* Textures whose mips were built before a usable palette existed. They are
 * rebuilt in SetPalette as soon as one appears (normally during the loading
 * fade), instead of stalling the first race frames. */
static br_pixelmap** mips_pending;
static int mips_pending_count, mips_pending_capacity;
static void ForgetPendingMips(br_pixelmap* pm) {
    for (int i = 0; i < mips_pending_count; i++) {
        if (mips_pending[i] == pm) {
            mips_pending[i] = mips_pending[--mips_pending_count];
            return;
        }
    }
}
static void RememberPendingMips(br_pixelmap* pm) {
    ForgetPendingMips(pm);
    if (mips_pending_count == mips_pending_capacity) {
        int capacity = mips_pending_capacity ? mips_pending_capacity * 2 : 64;
        br_pixelmap** grown = realloc(mips_pending, sizeof(*grown) * capacity);
        if (grown == NULL) return; /* falls back to the rebuild on first use */
        mips_pending = grown;
        mips_pending_capacity = capacity;
    }
    mips_pending[mips_pending_count++] = pm;
}
static void UploadIndexedTexture(br_pixelmap* pm, tStored_pixelmap* stored);
static tStored_pixelmap* EnsureTexture(br_pixelmap* pm);
static uint8_t* screen_buffer_flip_pixels;
static uint16_t* depth_buffer_flip_pixels;

static int render_width, render_height;
/* Supersample factor for the 3D framebuffer. The CPU pixmap (game canvas the
 * 2D HUD draws into) stays at render_width × render_height; the GL framebuffer
 * the 3D pass writes to is super_factor × that on each axis, then nearest-
 * neighbour downsampled into the pixmap on FlushBuffer so the existing 2D
 * compositing keeps working unchanged. The super-res framebuffer is also
 * kept around as a GPU texture for the high-res present path. */
static int super_factor = 1;
static int super_width, super_height;
static int vp_x, vp_y, vp_width, vp_height;

static br_pixelmap *last_colour_buffer, *last_depth_buffer;
static int dirty_buffers = 0;
#ifdef __ANDROID__
static int shadow_batch;
#endif

static tStored_material* current_material;
static br_pixelmap* current_shade_table;

static br_matrix4 view_matrix;
#ifdef __ANDROID__
static float frustum_planes[6][4];
#endif

// Increment flush_counter during flush, and upload_counter when we upload the colour_buffer texture.
// If the counters are equal, we can avoid re-uploading the same thing.
static unsigned int flush_counter = 0, colourbuffer_upload_counter = 0;

typedef struct gl_vertex {
    br_vector3 p;
    br_vector2 map;
    br_vector3 n;
    float colour_index; // float to allow interpolation
} gl_vertex;

struct {
    GLuint model, view, projection;
    // gl_NormalMatrix replacement
    GLuint normal_matrix;
    GLuint clip_plane_count;
    GLuint clip_planes[6];
    GLuint colour_buffer_texture;

    GLuint material_flags;
    GLuint material_texture_enabled;
    GLuint material_texture_pixelmap;
    GLuint material_uv_transform;
    GLuint material_shade_table;
    GLuint material_blend_enabled;
    GLuint material_blend_table;
    GLuint material_index_base;
    GLuint texture_filtering, palette, filter_lut;
    // GLuint material_index_range;  TODO: re-add when we add untextured lighting
    GLuint material_shade_table_height;

} uniforms_3d;


/* Spatial upscalers for the present path (DETHRACE_UPSCALER).
 * Both resolve the paletted composite to RGBA8 at the 3D (super) resolution
 * first, then:
 *  - fsr1: AMD FidelityFX Super Resolution 1.0. EASU upscales into an RGBA8
 *    target at viewport size, then RCAS sharpens it onto the window.
 *  - gsr1: Snapdragon Game Super Resolution 1. One pass that upscales and
 *    sharpens straight onto the window; cheaper than FSR's two passes.
 * Used only when the viewport is larger than the source; otherwise the regular
 * nearest present runs. */
typedef enum { UPSCALER_NONE, UPSCALER_FSR1, UPSCALER_GSR1 } tUpscaler;
static tUpscaler upscaler;
static GLuint fsr_easu_program, fsr_rcas_program, gsr_program;
static GLuint upscale_source_fbo, upscale_source_texture;
/* Source image for the upscaler: the super grid, SSAA-resolved down to the
 * viewport on any axis where the viewport is smaller (e.g. a wide screen
 * that is wider but shorter than the 3D image). Sized lazily. */
static int upscale_source_width, upscale_source_height;
static GLuint fsr_easu_fbo, fsr_easu_texture;
static int fsr_easu_width, fsr_easu_height;
static struct {
    GLint easu_source_size, easu_output_size;
    GLint rcas_source_size, rcas_offset;
    GLint gsr_source_size, gsr_output_size, gsr_offset;
} uniforms_upscale;

/* Optional post-processing on the RGBA composite (DETHRACE_FXAA,
 * DETHRACE_SHARPEN): FXAA on 3D pixels only, then RCAS sharpening. Runs on
 * the regular present path; with an upscaler, FXAA runs on its source and the
 * sharpening level drives FSR's own RCAS pass. */
typedef struct { GLuint fbo, texture; int width, height; } tPostTarget;
static tPostTarget post_a, post_b;
static int post_fxaa;
static float post_sharpness; /* exp2(-stops); 0 = no extra sharpening pass */
static GLuint fxaa_program;
static struct { GLint source_size, offset; } uniforms_fxaa;
static GLint rcas_sharpness;

/* defines: extra "#define X\n" lines for the fragment shader, placed right
 * after the #version header (may be empty). */
static GLuint CreateShaderProgramDefines(char* name, const char* vertex_shader, const int vertex_shader_len, const char* fragment_shader, const int fragment_shader_len, const char* defines) {
    int success;
    char log_buffer[1024];
    GLuint program;
    GLuint v_shader, f_shader;

    embedded_resource_t* header;

    if (opengl_profile == eOpenGL_profile_es) {
        header = get_embedded_resource_by_name("resources/gles_header.glsl");
    } else {
        header = get_embedded_resource_by_name("resources/glcore_header.glsl");
    }
    if (header == NULL) {
        LOG_PANIC("Failed to get glsl header resource");
    }

    program = glCreateProgram();
    v_shader = glCreateShader(GL_VERTEX_SHADER);
    const GLchar* vertex_sources[] = { header->content, vertex_shader };
    const GLint vertex_source_lengths[] = { header->len, vertex_shader_len };
    glShaderSource(v_shader, 2, vertex_sources, vertex_source_lengths);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(v_shader, 1024, NULL, log_buffer);
        LOG_PANIC("shader %s failed to compile: %s", name, log_buffer);
    }

    f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar* fragment_sources[] = { header->content, defines, fragment_shader };
    const GLint fragment_source_lengths[] = { header->len, (GLint)strlen(defines), fragment_shader_len };
    glShaderSource(f_shader, 3, fragment_sources, fragment_source_lengths);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log_buffer[1024];
        glGetShaderInfoLog(f_shader, 1024, NULL, log_buffer);
        LOG_PANIC("shader %s failed to compile: %s", name, log_buffer);
    }

    glAttachShader(program, v_shader);
    glAttachShader(program, f_shader);
    glLinkProgram(program);
    glDeleteShader(v_shader);
    glDeleteShader(f_shader);

    GLint link_ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
        glGetProgramInfoLog(program, 1024, NULL, log_buffer);
        LOG_PANIC("shader program %s failed to link: %s", name, log_buffer);
    }
    return program;
}

static GLint GetValidatedUniformLocation(GLuint program, char* uniform_name) {
    GLint location;
    location = glGetUniformLocation(program, uniform_name);
    if (location == -1) {
        LOG_PANIC("glGetUniformLocation(%d, %s) failed. Check the shader uniform names.", program, uniform_name);
    }
    return location;
}

static GLuint CreateShaderProgram(char* name, const char* vertex_shader, const int vertex_shader_len, const char* fragment_shader, const int fragment_shader_len) {
    return CreateShaderProgramDefines(name, vertex_shader, vertex_shader_len, fragment_shader, fragment_shader_len, "");
}

/* The present (composite) shader, compiled per feature set. */
typedef struct { GLuint id; GLint output_size, mark_scene; } tPresent_program;
static tPresent_program present_plain, present_ssaa;

static void BuildPresentProgram(tPresent_program* out, embedded_resource_t* vert, embedded_resource_t* frag,
    const char* defines, int ui_scaler, float gamma, float saturation, float scanlines) {
    out->id = CreateShaderProgramDefines("framebuffer", vert->content, vert->len, frag->content, frag->len, defines);
    glUseProgram(out->id);
    /* Samplers are always used; everything else may be compiled out, in
     * which case the location is -1 and glUniform* ignores it. */
    glUniform1i(GetValidatedUniformLocation(out->id, "u_pixels"), 0);
    glUniform1i(GetValidatedUniformLocation(out->id, "u_palette"), 1);
    glUniform1i(GetValidatedUniformLocation(out->id, "u_snapshot"), 5);
    glUniform1i(GetValidatedUniformLocation(out->id, "u_super_3d"), 6);
    glUniform1i(glGetUniformLocation(out->id, "u_smooth"), 0);
    out->output_size = glGetUniformLocation(out->id, "u_output_size");
    out->mark_scene = glGetUniformLocation(out->id, "u_mark_scene");
    glUniform1i(out->mark_scene, 0);
    glUniform1i(glGetUniformLocation(out->id, "u_ui_scaler"), ui_scaler);
    glUniform1f(glGetUniformLocation(out->id, "u_gamma"), gamma);
    glUniform1f(glGetUniformLocation(out->id, "u_saturation"), saturation);
    glUniform1f(glGetUniformLocation(out->id, "u_scanlines"), scanlines);
}

/* Bind the cheapest present variant for an output of w x h pixels: the SSAA
 * one only when the super-res 3D image is larger than the output on an axis
 * (otherwise averaging cannot change a single pixel). */
static tPresent_program* UsePresentProgram(int w, int h) {
    tPresent_program* program = present_ssaa.id && (w < super_width || h < super_height)
        ? &present_ssaa : &present_plain;
    glUseProgram(program->id);
    glUniform2f(program->output_size, (float)w, (float)h);
    return program;
}

static void LoadShaders(void) {
    embedded_resource_t* frag = get_embedded_resource_by_name("resources/framebuffer_frag.glsl");
    if (frag == NULL) {
        LOG_PANIC("Failed to get embedded resource");
    }
    embedded_resource_t* vert = get_embedded_resource_by_name("resources/framebuffer_vert.glsl");
    if (vert == NULL) {
        LOG_PANIC("Failed to get embedded resource");
    }
#ifdef __ANDROID__
    embedded_resource_t* downsample = get_embedded_resource_by_name("resources/readback_frag.glsl");
    if (!downsample) LOG_PANIC("Missing readback shader");
    readback_program = CreateShaderProgram("readback", vert->content, vert->len, downsample->content, downsample->len);
    glUseProgram(readback_program);
    glUniform1i(GetValidatedUniformLocation(readback_program, "source_pixels"), 0);
    readback_sample_factor = GetValidatedUniformLocation(readback_program, "sample_factor");
    readback_shadow_pixels = glGetUniformLocation(readback_program, "shadow_pixels");
    readback_use_shadow = glGetUniformLocation(readback_program, "use_shadow_pixels");
    glUniform1i(readback_sample_factor, super_factor);
#endif
    post_fxaa = 0;
    post_sharpness = 0.0f;
    const char* fxaa_name = getenv("DETHRACE_FXAA");
    post_fxaa = fxaa_name && strcmp(fxaa_name, "1") == 0;
    const char* sharpen_name = getenv("DETHRACE_SHARPEN");
    if (sharpen_name) {
        /* RCAS strength in stops below maximum: 1, 0.5 and 0. */
        if (strcmp(sharpen_name, "low") == 0) post_sharpness = 0.5f;
        else if (strcmp(sharpen_name, "medium") == 0) post_sharpness = 0.70710678f;
        else if (strcmp(sharpen_name, "high") == 0) post_sharpness = 1.0f;
    }
    {
        /* Presentation preferences (Android launcher; defaults = original look). */
        const char* ui = getenv("DETHRACE_UI_SCALER");
        const char* gamma = getenv("DETHRACE_GAMMA");
        const char* saturation = getenv("DETHRACE_SATURATION");
        const char* scanlines = getenv("DETHRACE_SCANLINES");
        int ui_scaler = ui && strcmp(ui, "sharp") == 0 ? 1 : ui && strcmp(ui, "smooth") == 0 ? 2 : 0;
        float gamma_value = gamma && strcmp(gamma, "darker") == 0 ? 0.9f
            : gamma && strcmp(gamma, "bright") == 0 ? 1.15f
            : gamma && strcmp(gamma, "brighter") == 0 ? 1.3f : 1.0f;
        float saturation_value = saturation && strcmp(saturation, "vivid") == 0 ? 1.25f : 1.0f;
        float scanline_value = scanlines && strcmp(scanlines, "light") == 0 ? 0.25f
            : scanlines && strcmp(scanlines, "strong") == 0 ? 0.45f : 0.0f;
        /* Compile only the features in use: at defaults the present shader
         * does the same work as 0.9.2 (one composite lookup per pixel).
         * SSAA averaging only matters when the 3D image is supersampled;
         * DETHRACE_SSAA=off restores 0.9.2's single-sample present. */
        const char* ssaa = getenv("DETHRACE_SSAA");
        int use_ssaa = super_factor > 1 && !(ssaa && strcmp(ssaa, "off") == 0);
        int use_color = gamma_value != 1.0f || saturation_value != 1.0f || scanline_value > 0.0f;
        char defines[256];
        snprintf(defines, sizeof(defines), "%s%s%s",
                 ui_scaler ? "#define PRESENT_UI_SCALER\n" : "",
                 use_color ? "#define PRESENT_COLOR_ADJUST\n" : "",
                 post_fxaa ? "#define PRESENT_SCENE_MASK\n" : "");
        BuildPresentProgram(&present_plain, vert, frag, defines, ui_scaler, gamma_value, saturation_value, scanline_value);
        present_ssaa.id = 0;
        if (use_ssaa) {
            /* Second variant, used only on frames where the 3D image is
             * actually larger than the output (see UsePresentProgram). */
            char ssaa_defines[300];
            snprintf(ssaa_defines, sizeof(ssaa_defines), "#define PRESENT_SSAA\n%s", defines);
            BuildPresentProgram(&present_ssaa, vert, frag, ssaa_defines, ui_scaler, gamma_value, saturation_value, scanline_value);
        }
        shader_program_2d = present_plain.id;
        LOG_INFO("Presentation: SSAA %d, UI scaler %d, gamma %.2f, saturation %.2f, scanlines %.2f, FXAA mask %d",
                 use_ssaa, ui_scaler, gamma_value, saturation_value, scanline_value, post_fxaa);
    }
    texture_filtering = 0;
#ifdef __ANDROID__
    const char* filtering = getenv("DETHRACE_IMAGE_FILTER");
    /* 1 = Smooth (bilinear), 2 = Crisp (solid texels, anti-aliased edges). */
    texture_filtering = filtering && strcmp(filtering, "smooth") == 0 ? 1
        : filtering && strcmp(filtering, "crisp") == 0 ? 2 : 0;
#endif

    upscaler = UPSCALER_NONE;
    const char* upscaler_name = getenv("DETHRACE_UPSCALER");
    if (upscaler_name && strcmp(upscaler_name, "fsr1") == 0) {
        embedded_resource_t* easu = get_embedded_resource_by_name("resources/fsr_easu_frag.glsl");
        if (!easu) LOG_PANIC("Missing FSR shaders");
        fsr_easu_program = CreateShaderProgram("fsr_easu", vert->content, vert->len, easu->content, easu->len);
        glUseProgram(fsr_easu_program);
        glUniform1i(GetValidatedUniformLocation(fsr_easu_program, "u_source"), 0);
        uniforms_upscale.easu_source_size = GetValidatedUniformLocation(fsr_easu_program, "u_source_size");
        uniforms_upscale.easu_output_size = GetValidatedUniformLocation(fsr_easu_program, "u_output_size");
        upscaler = UPSCALER_FSR1;
        LOG_INFO("Upscaler: AMD FSR 1.0 (EASU + RCAS)");
    } else if (upscaler_name && strcmp(upscaler_name, "gsr1") == 0) {
        embedded_resource_t* gsr = get_embedded_resource_by_name("resources/gsr_frag.glsl");
        if (!gsr) LOG_PANIC("Missing GSR shader");
        gsr_program = CreateShaderProgram("gsr", vert->content, vert->len, gsr->content, gsr->len);
        glUseProgram(gsr_program);
        glUniform1i(GetValidatedUniformLocation(gsr_program, "u_source"), 0);
        uniforms_upscale.gsr_source_size = GetValidatedUniformLocation(gsr_program, "u_source_size");
        uniforms_upscale.gsr_output_size = GetValidatedUniformLocation(gsr_program, "u_output_size");
        uniforms_upscale.gsr_offset = GetValidatedUniformLocation(gsr_program, "u_offset");
        upscaler = UPSCALER_GSR1;
        LOG_INFO("Upscaler: Snapdragon GSR 1");
    }

    if (post_fxaa) {
        embedded_resource_t* fxaa = get_embedded_resource_by_name("resources/fxaa_frag.glsl");
        if (!fxaa) LOG_PANIC("Missing FXAA shader");
        fxaa_program = CreateShaderProgram("fxaa", vert->content, vert->len, fxaa->content, fxaa->len);
        glUseProgram(fxaa_program);
        glUniform1i(GetValidatedUniformLocation(fxaa_program, "u_source"), 0);
        uniforms_fxaa.source_size = GetValidatedUniformLocation(fxaa_program, "u_source_size");
        uniforms_fxaa.offset = GetValidatedUniformLocation(fxaa_program, "u_offset");
        LOG_INFO("Post-processing: FXAA");
    }
    if (upscaler == UPSCALER_FSR1 || post_sharpness > 0.0f) {
        embedded_resource_t* rcas = get_embedded_resource_by_name("resources/fsr_rcas_frag.glsl");
        if (!rcas) LOG_PANIC("Missing RCAS shader");
        fsr_rcas_program = CreateShaderProgram("fsr_rcas", vert->content, vert->len, rcas->content, rcas->len);
        glUseProgram(fsr_rcas_program);
        glUniform1i(GetValidatedUniformLocation(fsr_rcas_program, "u_source"), 0);
        uniforms_upscale.rcas_source_size = GetValidatedUniformLocation(fsr_rcas_program, "u_source_size");
        uniforms_upscale.rcas_offset = GetValidatedUniformLocation(fsr_rcas_program, "u_offset");
        rcas_sharpness = GetValidatedUniformLocation(fsr_rcas_program, "u_sharpness");
        /* FSR without a chosen level keeps AMD's default: 0.2 stops below max. */
        glUniform1f(rcas_sharpness, post_sharpness > 0.0f ? post_sharpness : 0.87055056f);
        if (post_sharpness > 0.0f) LOG_INFO("Post-processing: RCAS sharpening %.2f", post_sharpness);
    }

    frag = get_embedded_resource_by_name("resources/3d_frag.glsl");
    if (frag == NULL) {
        LOG_PANIC("Failed to get embedded resource");
    }
    vert = get_embedded_resource_by_name("resources/3d_vert.glsl");
    if (vert == NULL) {
        LOG_PANIC("Failed to get embedded resource");
    }

    shader_program_3d = CreateShaderProgram("3d", vert->content, vert->len, frag->content, frag->len);
    glUseProgram(shader_program_3d);
    uniforms_3d.clip_plane_count = GetValidatedUniformLocation(shader_program_3d, "u_clip_plane_count");
    for (int i = 0; i < 6; i++) {
        char name[32];
        sprintf(name, "u_clip_planes[%d]", i);
        uniforms_3d.clip_planes[i] = GetValidatedUniformLocation(shader_program_3d, name);
    }

    uniforms_3d.model = GetValidatedUniformLocation(shader_program_3d, "u_model");
    uniforms_3d.colour_buffer_texture = GetValidatedUniformLocation(shader_program_3d, "u_colour_buffer");
    uniforms_3d.projection = GetValidatedUniformLocation(shader_program_3d, "u_projection");
    uniforms_3d.view = GetValidatedUniformLocation(shader_program_3d, "u_view");
    uniforms_3d.normal_matrix = GetValidatedUniformLocation(shader_program_3d, "u_normal_matrix");

    uniforms_3d.material_flags = GetValidatedUniformLocation(shader_program_3d, "u_material_flags");
    uniforms_3d.material_texture_enabled = GetValidatedUniformLocation(shader_program_3d, "u_material_texture_enabled");
    uniforms_3d.material_texture_pixelmap = GetValidatedUniformLocation(shader_program_3d, "u_material_texture_pixelmap");
    uniforms_3d.material_uv_transform = GetValidatedUniformLocation(shader_program_3d, "u_material_uv_transform");
    uniforms_3d.material_shade_table = GetValidatedUniformLocation(shader_program_3d, "u_material_shade_table");
    uniforms_3d.material_shade_table_height = GetValidatedUniformLocation(shader_program_3d, "u_material_shade_table_height");
    uniforms_3d.material_blend_enabled = GetValidatedUniformLocation(shader_program_3d, "u_material_blend_enabled");
    uniforms_3d.material_blend_table = GetValidatedUniformLocation(shader_program_3d, "u_material_blend_table");
    uniforms_3d.material_index_base = GetValidatedUniformLocation(shader_program_3d, "u_material_index_base");
    // TODO: re-add when we support untextured lighting
    // uniforms_3d.material_index_range = GetValidatedUniformLocation(shader_program_3d, "u_material_index_range");

    uniforms_3d.texture_filtering = GetValidatedUniformLocation(shader_program_3d, "u_texture_filtering");
    uniforms_3d.palette = GetValidatedUniformLocation(shader_program_3d, "u_material_palette");
    uniforms_3d.filter_lut = GetValidatedUniformLocation(shader_program_3d, "u_filter_lut");
    glUniform1i(uniforms_3d.texture_filtering, texture_filtering);
    glUniform1i(uniforms_3d.palette, 1);
    glUniform1i(uniforms_3d.filter_lut, 7);
    filter_palette_scale_location = GetValidatedUniformLocation(shader_program_3d, "u_palette_scale");
    glUniform1f(filter_palette_scale_location, 1.0f);
    /* DETHRACE_ANISO: probes along shallow-angle surfaces (Smooth only). */
    int anisotropy = 1;
#ifdef __ANDROID__
    const char* aniso = getenv("DETHRACE_ANISO");
    if (texture_filtering && aniso) anisotropy = atoi(aniso);
#endif
    if (anisotropy < 1) anisotropy = 1;
    if (anisotropy > 4) anisotropy = 4;
    glUniform1i(GetValidatedUniformLocation(shader_program_3d, "u_max_anisotropy"), anisotropy);
    if (texture_filtering) LOG_INFO("Texture filtering: %s trilinear, anisotropy %dx",
                                    texture_filtering == 2 ? "crisp" : "smooth", anisotropy);
    // bind the uniform samplers to texture units
    glUniform1i(uniforms_3d.material_texture_pixelmap, 0);
    glUniform1i(uniforms_3d.material_shade_table, 2);
    glUniform1i(uniforms_3d.material_blend_table, 3);
    glUniform1i(uniforms_3d.colour_buffer_texture, 4);
}

static void SetupFullScreenRectGeometry(void) {
    float vertices[] = {
        // positions          // colors           // texture coords
        1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
        1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,  // bottom right
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
        -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // top left
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };

    GLuint vbo;
    glGenVertexArrays(1, &screen_buffer_vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &screen_buffer_ebo);

    glBindVertexArray(screen_buffer_vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, screen_buffer_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void GLRenderer_SetSuperFactor(int factor) {
    if (factor < 1) {
        factor = 1;
    }
    super_factor = factor;
}

void GLRenderer_Init(tOpenGL_profile profile, int pRender_width, int pRender_height) {
    DRProfileGPU((const char*)glGetString(GL_VENDOR),(const char*)glGetString(GL_RENDERER),(const char*)glGetString(GL_VERSION));
    opengl_profile = profile;
#ifdef __ANDROID__
    shadow_batch = 0;
    pending_shadow = 0;
#endif
    flush_counter = 1;
    colourbuffer_upload_counter = 0;
    dirty_buffers = 0;
    render_width = pRender_width;
    render_height = pRender_height;
    if (super_factor < 1) {
        super_factor = 1;
    }
    {
        /* Ultra (4x) can exceed what older GLES 3 GPUs allow (2048 minimum);
         * step down until every super-resolution target fits. */
        GLint max_texture = 0, max_renderbuffer = 0, max_viewport[2] = { 0, 0 };
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer);
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport);
        int limit = max_texture;
        if (max_renderbuffer > 0 && max_renderbuffer < limit) limit = max_renderbuffer;
        if (max_viewport[0] > 0 && max_viewport[0] < limit) limit = max_viewport[0];
        if (max_viewport[1] > 0 && max_viewport[1] < limit) limit = max_viewport[1];
        int requested = super_factor;
        while (limit > 0 && super_factor > 1
               && (render_width * super_factor > limit || render_height * super_factor > limit)) {
            super_factor--;
        }
        if (super_factor != requested)
            LOG_WARN("3D resolution %dx reduced to %dx (GPU limit %d px)", requested, super_factor, limit);
    }
#ifdef __ANDROID__
    {
        const char* mirror_res = getenv("DETHRACE_MIRROR_RES");
        mirror_factor = mirror_res && strcmp(mirror_res, "full") == 0 ? super_factor : 1;
    }
#endif
    super_width = render_width * super_factor;
    super_height = render_height * super_factor;

    LOG_INFO("OpenGL vendor string: %s", glGetString(GL_VENDOR));
    LOG_INFO("OpenGL renderer string: %s", glGetString(GL_RENDERER));
    LOG_INFO("OpenGL version string: %s", glGetString(GL_VERSION));
    LOG_INFO("OpenGL shading language version string: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    int maxTextureImageUnits;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
    if (maxTextureImageUnits < 8) {
        LOG_PANIC("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS is %d. Need at least 8", maxTextureImageUnits);
    }

    LoadShaders();
    SetupFullScreenRectGeometry();

    // config
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // textures
    glGenTextures(1, &fullscreen_quad_texture);
    glGenTextures(1, &palette_texture);
    glGenTextures(1, &filter_lut_texture);
    filter_lut_ready = 0;
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, filter_lut_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Give the sampler complete storage even when filtering is disabled.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, 256, 128, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &framebuffer_texture);
    glGenTextures(1, &depth_texture);
    glGenTextures(1, &current_colourbuffer_texture);
    glGenTextures(1, &snapshot_texture);

    glBindTexture(GL_TEXTURE_2D, snapshot_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, render_width, render_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // setup framebuffer
    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);

    // set pixel storage alignment to 1 byte
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, render_width, render_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, palette_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, super_width, super_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer_texture, 0);

    glBindTexture(GL_TEXTURE_2D, current_colourbuffer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, super_width, super_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, depth_texture);
#ifdef __ANDROID__
    // More separation for nearby road/shadow surfaces on mobile GPUs.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, super_width, super_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, super_width, super_height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
#endif
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_PANIC("Framebuffer is not complete!");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#ifdef __ANDROID__
    glGenTextures(1, &deferred_shadow_texture);
    glBindTexture(GL_TEXTURE_2D, deferred_shadow_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, super_width, super_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &readback_framebuffer);
    glGenTextures(1, &readback_texture);
    glBindTexture(GL_TEXTURE_2D, readback_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, render_width, render_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, readback_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, readback_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        LOG_PANIC("Readback framebuffer is not complete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
#ifdef __ANDROID__
    // Mirror has a separate colour/depth target. Never overwrite the main
    // high-resolution scene while rendering this second camera.
    glGenFramebuffers(1,&mirror_fbo);glBindFramebuffer(GL_FRAMEBUFFER,mirror_fbo);
    glGenTextures(1,&mirror_colour);glBindTexture(GL_TEXTURE_2D,mirror_colour);
    /* Sized for the mirror's own render scale (1x by default), not the
     * main super resolution: saves ~25 MB of GPU memory at Ultra. */
    glTexImage2D(GL_TEXTURE_2D,0,GL_R8UI,render_width*mirror_factor,render_height*mirror_factor,0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,mirror_colour,0);
    glGenTextures(1,&mirror_depth);glBindTexture(GL_TEXTURE_2D,mirror_depth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,render_width*mirror_factor,render_height*mirror_factor,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,mirror_depth,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)LOG_PANIC("Mirror framebuffer incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER,0);
#endif
    if (upscaler != UPSCALER_NONE) {
        glGenFramebuffers(1, &upscale_source_fbo);
        glGenTextures(1, &upscale_source_texture);
        upscale_source_width = upscale_source_height = 0; /* sized lazily */
        if (upscaler == UPSCALER_FSR1) {
            glGenFramebuffers(1, &fsr_easu_fbo);
            glGenTextures(1, &fsr_easu_texture);
            fsr_easu_width = fsr_easu_height = 0; /* sized lazily to the viewport */
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
#ifdef __ANDROID__
    /* Android reads back at canvas resolution and never reads depth. */
    screen_buffer_flip_pixels = malloc(sizeof(uint8_t) * render_width * render_height);
    depth_buffer_flip_pixels = NULL;
#else
    screen_buffer_flip_pixels = malloc(sizeof(uint8_t) * super_width * super_height);
    depth_buffer_flip_pixels = malloc(sizeof(uint16_t) * super_width * super_height);
#endif
    snapshot_pixels = calloc(render_width * render_height, sizeof(uint8_t));

    CHECK_GL_ERROR("initializeOpenGLContext");
}

void GLRenderer_SetPalette(uint8_t* rgba_colors) {
    for (int i = 0; i < 256; i++) {
        gl_palette[i * 4] = rgba_colors[i * 4 + 2];
        gl_palette[i * 4 + 1] = rgba_colors[i * 4 + 1];
        gl_palette[i * 4 + 2] = rgba_colors[i * 4];
        // palette index 0 is transparent, so set alpha to 0
        if (i == 0) {
            gl_palette[i * 4 + 3] = 0;
        } else {
            gl_palette[i * 4 + 3] = 0xff;
        }
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, palette_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, gl_palette);

    if (texture_filtering && DRPaletteUsableForMips(gl_palette)) {
        memcpy(mip_palette, gl_palette, sizeof(gl_palette));
        mip_palette_valid = 1;
        if (mips_pending_count > 0) {
            glActiveTexture(GL_TEXTURE0);
            while (mips_pending_count > 0) {
                br_pixelmap* pm = mips_pending[mips_pending_count - 1];
                /* UploadIndexedTexture clears the flag; drop it either way. */
                mips_pending_count--;
                if (pm->stored != NULL && ((tStored_pixelmap*)pm->stored)->mips_need_palette)
                    UploadIndexedTexture(pm, pm->stored);
            }
            current_material = NULL; /* unit 0 binding changed: force a rebind */
        }
    }

    if (texture_filtering) {
        float scale = filter_lut_ready ? DRPaletteFadeScale(filter_lut_palette, gl_palette) : -1.f;
        if (scale < 0.f && filter_lut_ready && !DRPaletteUsableForMips(gl_palette)) {
            /* Too dark to rank colours (fade-in from black, loading screens):
             * every index looks black anyway, so keep the existing LUT
             * rather than paying a ~8M-test rebuild per fade step. */
            scale = 1.f / 64.f;
        }
        if (scale < 0.f) {
            uint8_t lut[DR_PALETTE_LUT_SIZE];
            DRBuildPaletteLUT5(lut, gl_palette);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, filter_lut_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 128, GL_RED_INTEGER, GL_UNSIGNED_BYTE, lut);
            memcpy(filter_lut_palette, gl_palette, sizeof(gl_palette));
            filter_lut_ready = 1;
            scale = 1.f;
        }
        /* Near-black fades: any index displays as black, avoid dividing by 0. */
        if (scale < 1.f / 64.f) scale = 1.f / 64.f;
        /* Applied at the next BeginScene: no GL state query or program
         * switch here (fades set many palettes per frame). */
        filter_palette_scale = scale;
    }
    // reset active texture back to default
    glActiveTexture(GL_TEXTURE0);

    CHECK_GL_ERROR("GLRenderer_SetPalette");
}

void GLRenderer_SetShadeTable(br_pixelmap* table) {
    if (current_shade_table == table) {
        return;
    }

    // shade table uses texture unit 2
    tStored_pixelmap* stored = EnsureTexture(table);
    if (stored == NULL) return;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, stored->id);

    // reset active texture back to default
    glActiveTexture(GL_TEXTURE0);
    current_shade_table = table;
}

void GLRenderer_SetBlendTable(br_pixelmap* table) {

    if (flush_counter != colourbuffer_upload_counter) {
        // Snapshot the super-res framebuffer into the colour-buffer texture
        // BEFORE FlushBuffer clears it. The 3D shader uses the same bottom-up
        // super-resolution gl_FragCoord coordinates for the indexed blend lookup.
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, current_colourbuffer_texture);
#ifdef __ANDROID__
        /* Blended fragments only read their own position (gl_FragCoord)
         * inside the current scene viewport: copy just that rectangle. */
        if (scene_viewport[2] > 0 && scene_viewport[3] > 0) {
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, scene_viewport[0], scene_viewport[1],
                scene_viewport[0], scene_viewport[1], scene_viewport[2], scene_viewport[3]);
        } else
#endif
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, super_width, super_height);
        glActiveTexture(GL_TEXTURE0);
#ifndef __ANDROID__
        GLRenderer_FlushBuffer(eFlush_color_buffer);
#endif
        /* Android: the snapshot is already GPU-resident. Keep accumulated
         * colour/depth until an actual CPU consumer requests FlushBuffers.
         * No readback or clear is necessary to sample a separate texture. */
        colourbuffer_upload_counter = flush_counter;
    }

    // blend table uses texture unit 3
    tStored_pixelmap* stored = EnsureTexture(table);
    if (stored == NULL) return;
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, stored->id);

    // reset active texture back to default
    glActiveTexture(GL_TEXTURE0);
}

extern br_v1db_state v1db;

void GLRenderer_BeginScene(br_actor* camera, br_pixelmap* colour_buffer, br_pixelmap* depth_buffer) {
#ifdef __ANDROID__
    extern br_pixelmap* gRearview_screen;
    mirror_scene=colour_buffer==gRearview_screen;
    if(mirror_scene) {
        SwapMirrorTarget();
        static const GLuint zero[4]={0,0,0,0};
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_id);
        glClearBufferuiv(GL_COLOR,0,zero);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);
    }
    // The retail mirror camera contains a reflection (negative determinant).
    glFrontFace(mirror_scene?GL_CW:GL_CCW);
#endif
    last_colour_buffer = colour_buffer;
    last_depth_buffer = depth_buffer;
    /* The 3D pass renders into a super-res framebuffer, so scale viewport
     * dimensions and y-flip math by super_factor. */
    int scene_factor = super_factor;
#ifdef __ANDROID__
    /* The mirror only reaches the screen through the CPU pixmap at canvas
     * resolution, so supersampling it is wasted work: draw it at 1x. */
    if (mirror_scene) scene_factor = mirror_factor;
#endif
    scene_viewport[0] = colour_buffer->base_x * scene_factor;
    scene_viewport[1] = (render_height - colour_buffer->height - colour_buffer->base_y) * scene_factor;
    scene_viewport[2] = colour_buffer->width * scene_factor;
    scene_viewport[3] = colour_buffer->height * scene_factor;
    glViewport(scene_viewport[0], scene_viewport[1], scene_viewport[2], scene_viewport[3]);
    glUseProgram(shader_program_3d);
    if (texture_filtering) glUniform1f(filter_palette_scale_location, filter_palette_scale);

    br_camera* cam = camera->type_data;
    current_material = NULL;
    current_shade_table = NULL;

    // clip planes
    int enabled_clip_planes = 0;
    for (int i = 0; i < v1db.enabled_clip_planes.max; i++) {
        if (v1db.enabled_clip_planes.enabled == NULL || !v1db.enabled_clip_planes.enabled[i]) {
            continue;
        }
        br_vector4* v4 = v1db.enabled_clip_planes.enabled[i]->type_data;
        glUniform4f(uniforms_3d.clip_planes[enabled_clip_planes], v4->v[0], v4->v[1], v4->v[2], v4->v[3]);
        enabled_clip_planes++;
    }
    glUniform1i(uniforms_3d.clip_plane_count, enabled_clip_planes);

    // view matrix
    BrMatrix4Copy34(&view_matrix, &v1db.camera_path[0].m);
    BrMatrix4Inverse(&view_matrix, &view_matrix);
    glUniformMatrix4fv(uniforms_3d.view, 1, GL_FALSE, &view_matrix.m[0][0]);

    // projection matrix
    br_matrix4 projection;
    BrMatrix4Perspective(&projection, cam->field_of_view, cam->aspect, cam->hither_z, cam->yon_z);
    // hack: not sure why we have to do this, but this makes the result the same as `glm_perspective`
    projection.m[2][2] *= -1;
    glUniformMatrix4fv(uniforms_3d.projection, 1, GL_FALSE, &projection.m[0][0]);
#ifdef __ANDROID__
    DRFrustumPlanes(frustum_planes, view_matrix.m, projection.m);
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    CHECK_GL_ERROR("GLRenderer_BeginScene");
}

void GLRenderer_EndScene(void) {
#ifdef __ANDROID__
    if(mirror_scene){SwapMirrorTarget();mirror_scene=0;}
    glFrontFace(GL_CCW);
#endif
    //  switch back to default fb and reset state
    CHECK_GL_ERROR("GLRenderer_EndScene2");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthMask(GL_TRUE);
    glDepthMask(GL_TRUE);
    CHECK_GL_ERROR("GLRenderer_EndScene");
}

/* (Re)allocate a post-processing RGBA8 target; 0 if the GPU refuses it. */
static int PostTargetPrepare(tPostTarget* target, int width, int height) {
    if (target->fbo && target->width == width && target->height == height) return 1;
    if (!target->fbo) {
        glGenFramebuffers(1, &target->fbo);
        glGenTextures(1, &target->texture);
    }
    glBindTexture(GL_TEXTURE_2D, target->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* FXAA and GSR sample between texels; RCAS uses texelFetch. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, target->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
    int ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) {
        LOG_WARN("Post-processing target %dx%d incomplete; disabling FXAA/sharpening", width, height);
        post_fxaa = 0;
        post_sharpness = 0.0f;
        return 0;
    }
    target->width = width;
    target->height = height;
    return 1;
}

/* FXAA from source (w x h) into the bound framebuffer at window offset x/y.
 * Leaves the screen quad VAO bound. */
static void DrawFxaa(GLuint source, int width, int height, int x, int y) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glViewport(x, y, width, height);
    glUseProgram(fxaa_program);
    glUniform2f(uniforms_fxaa.source_size, (float)width, (float)height);
    glUniform2f(uniforms_fxaa.offset, (float)x, (float)y);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

/* (Re)allocate the upscaler source when the viewport size changes. */
static int UpscalePrepareSource(void) {
    int w = vp_width < super_width ? vp_width : super_width;
    int h = vp_height < super_height ? vp_height : super_height;
    if (w == upscale_source_width && h == upscale_source_height) return 1;
    glBindTexture(GL_TEXTURE_2D, upscale_source_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    /* GSR's base colour is a bilinear sample; FSR only uses texelFetch. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, upscale_source_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, upscale_source_texture, 0);
    int ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) {
        LOG_WARN("Upscaler source %dx%d incomplete; using regular present", w, h);
        upscaler = UPSCALER_NONE;
        return 0;
    }
    upscale_source_width = w;
    upscale_source_height = h;
    return 1;
}

/* (Re)allocate the EASU target when the viewport size changes. */
static int FsrPrepareTarget(void) {
    if (vp_width == fsr_easu_width && vp_height == fsr_easu_height) return 1;
    glBindTexture(GL_TEXTURE_2D, fsr_easu_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vp_width, vp_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fsr_easu_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fsr_easu_texture, 0);
    int ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) {
        LOG_WARN("FSR target %dx%d incomplete; using nearest present", vp_width, vp_height);
        upscaler = UPSCALER_NONE;
        return 0;
    }
    fsr_easu_width = vp_width;
    fsr_easu_height = vp_height;
    return 1;
}

/* Composite -> RGBA8 at super resolution, then FSR (EASU -> viewport-size
 * target, RCAS -> window) or GSR (single pass -> window).
 * Expects the composite inputs already bound to units 0/1/5/6. */
static void UpscalePresent(void) {
    /* FsrPrepareTarget may have rebound unit 0; restore the pixmap. */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
    glBindVertexArray(screen_buffer_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, screen_buffer_ebo);

    const float src_w = (float)upscale_source_width, src_h = (float)upscale_source_height;
    glBindFramebuffer(GL_FRAMEBUFFER, upscale_source_fbo);
    glViewport(0, 0, upscale_source_width, upscale_source_height);
    /* Nearest on axes kept at super size, SSAA on axes shrunk to the viewport. */
    tPresent_program* present = UsePresentProgram(upscale_source_width, upscale_source_height);
    glUniform1i(present->mark_scene, post_fxaa);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glUniform1i(present->mark_scene, 0);

    GLuint upscale_input = upscale_source_texture;
    if (post_fxaa && PostTargetPrepare(&post_b, upscale_source_width, upscale_source_height)) {
        /* Anti-alias before upscaling, so FSR/GSR enlarge smooth edges. */
        glBindFramebuffer(GL_FRAMEBUFFER, post_b.fbo);
        DrawFxaa(upscale_source_texture, upscale_source_width, upscale_source_height, 0, 0);
        upscale_input = post_b.texture;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, upscale_input);
    if (upscaler == UPSCALER_GSR1) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vp_x, vp_y, vp_width, vp_height);
        glUseProgram(gsr_program);
        glUniform2f(uniforms_upscale.gsr_source_size, src_w, src_h);
        glUniform2f(uniforms_upscale.gsr_output_size, (float)vp_width, (float)vp_height);
        glUniform2f(uniforms_upscale.gsr_offset, (float)vp_x, (float)vp_y);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
        glBindVertexArray(0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fsr_easu_fbo);
    glViewport(0, 0, vp_width, vp_height);
    glUseProgram(fsr_easu_program);
    glUniform2f(uniforms_upscale.easu_source_size, src_w, src_h);
    glUniform2f(uniforms_upscale.easu_output_size, (float)vp_width, (float)vp_height);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindTexture(GL_TEXTURE_2D, fsr_easu_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(vp_x, vp_y, vp_width, vp_height);
    glUseProgram(fsr_rcas_program);
    glUniform2f(uniforms_upscale.rcas_source_size, (float)vp_width, (float)vp_height);
    glUniform2f(uniforms_upscale.rcas_offset, (float)vp_x, (float)vp_y);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
    glBindVertexArray(0);
}

void GLRenderer_FullScreenQuad(uint8_t* screen_buffer) {
    uint64_t profile_present = DRProfileClock();

#ifdef __ANDROID__
    // Force the composited mirror rectangle through the CPU image, even where
    // a mirror palette index happens to equal the main-scene snapshot index.
    {
        /* Clamp once instead of testing every pixel. */
        int x0 = cpu_rect[0] < 0 ? 0 : cpu_rect[0], y0 = cpu_rect[1] < 0 ? 0 : cpu_rect[1];
        int x1 = cpu_rect[0] + cpu_rect[2], y1 = cpu_rect[1] + cpu_rect[3];
        if (x1 > render_width) x1 = render_width;
        if (y1 > render_height) y1 = render_height;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++)
                snapshot_pixels[y * render_width + x] = screen_buffer[y * render_width + x] ^ 255;
        if (x1 > x0 && y1 > y0) snapshot_dirty = snapshot_nonzero = 1;
    }
#endif
    glViewport(vp_x, vp_y, vp_width, vp_height);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDisable(GL_DEPTH_TEST);

    // unit 0: current pixmap (R8UI)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, screen_buffer);

    // unit 5: post-3D-readback snapshot (R8UI). If the snapshot hasn't been
    // populated this frame (no 3D pass), it is all zeros from ClearBuffers.
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, snapshot_texture);
    if (snapshot_dirty) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, snapshot_pixels);
        snapshot_dirty = 0;
    }

    // unit 6: super-res 3D framebuffer (R8UI), already on the GPU from the
    // 3D pass; no upload needed.
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);

    // restore active unit so subsequent draws aren't surprised
    glActiveTexture(GL_TEXTURE0);

    /* Upscalers only: take this path when the viewport is larger than the
     * source (e.g. Standard/Performance 3D resolution). */
    if (upscaler != UPSCALER_NONE && (vp_width > super_width || vp_height > super_height)
        && UpscalePrepareSource() && (upscaler != UPSCALER_FSR1 || FsrPrepareTarget())) {
        UpscalePresent();
        glEnable(GL_DEPTH_TEST);
        CHECK_GL_ERROR("GLRenderer_RenderFullScreenQuad (upscaler)");
        DRProfileTime(DRP_PRESENT, profile_present);
        return;
    }

    glBindVertexArray(screen_buffer_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, screen_buffer_ebo);
    /* Box-filters the super-res 3D grid down to the screen (real SSAA) when
     * it is larger than the screen; otherwise the plain composite shader. */
    tPresent_program* present = UsePresentProgram(vp_width, vp_height);
    if ((post_fxaa || post_sharpness > 0.0f) && PostTargetPrepare(&post_a, vp_width, vp_height)
        && (!(post_fxaa && post_sharpness > 0.0f) || PostTargetPrepare(&post_b, vp_width, vp_height))) {
        /* Composite (with the 3D mask in alpha) -> post_a, then
         * FXAA -> post_b or screen, then RCAS -> screen.
         * PostTargetPrepare may have rebound unit 0; restore the pixmap. */
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
        glBindFramebuffer(GL_FRAMEBUFFER, post_a.fbo);
        glViewport(0, 0, vp_width, vp_height);
        glUseProgram(present->id);
        glUniform1i(present->mark_scene, post_fxaa);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glUniform1i(present->mark_scene, 0);
        GLuint current = post_a.texture;
        if (post_fxaa) {
            int last = post_sharpness <= 0.0f;
            glBindFramebuffer(GL_FRAMEBUFFER, last ? 0 : post_b.fbo);
            DrawFxaa(current, vp_width, vp_height, last ? vp_x : 0, last ? vp_y : 0);
            current = post_b.texture;
        }
        if (post_sharpness > 0.0f) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, current);
            glViewport(vp_x, vp_y, vp_width, vp_height);
            glUseProgram(fsr_rcas_program);
            glUniform2f(uniforms_upscale.rcas_source_size, (float)vp_width, (float)vp_height);
            glUniform2f(uniforms_upscale.rcas_offset, (float)vp_x, (float)vp_y);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
        glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fullscreen_quad_texture);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(vp_x, vp_y, vp_width, vp_height);
        glUseProgram(present->id);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    CHECK_GL_ERROR("GLRenderer_RenderFullScreenQuad");
    DRProfileTime(DRP_PRESENT, profile_present);
}

void GLRenderer_ClearBuffers(void) {
#ifdef __ANDROID__
    pending_shadow = 0;
    memset(cpu_rect,0,sizeof(cpu_rect));
#endif
    // clear our virtual framebuffer. The colour attachment is GL_R8UI so
    // glClear/glClearColor (which are float-typed) do not portably write a
    // defined value on GLES; use glClearBufferuiv instead. Without this the
    // super-res 3D framebuffer carries content from previous frames and the
    // present shader displays the residue as ghost geometry.
    static const GLuint zero[4] = { 0, 0, 0, 0 };
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glClearBufferuiv(GL_COLOR, 0, zero);
    glClear(GL_DEPTH_BUFFER_BIT);

    // clear real framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Reset the post-3D-readback snapshot. If FlushBuffer doesn't run this
    // frame (frontend menu, no 3D pass) the present shader will see snapshot=0
    // everywhere, treat all non-zero pixmap pixels as 2D draws and skip the
    // super-res 3D path entirely.
    if (snapshot_pixels != NULL && snapshot_nonzero) {
        memset(snapshot_pixels, 0, render_width * render_height);
        snapshot_nonzero = 0;
        snapshot_dirty = 1;
    }

    CHECK_GL_ERROR("GLRenderer_ClearBuffers");
}

void GLRenderer_BufferModel(br_model* model) {
    DRProfileCount(DRP_UPLOADS,1);
    tStored_model_context* ctx;
    v11model* v11;

    if (model->stored != NULL) {
        LOG_PANIC("trying to build a stored model");
    }

    v11 = model->prepared;
    ctx = NewStoredModelContext();

    int total_verts = 0, total_faces = 0;
    for (int i = 0; i < v11->ngroups; i++) {
        total_verts += v11->groups[i].nvertices;
        total_faces += v11->groups[i].nfaces;
    }

    // override normals
    br_vector3 v3 = { { 0, 0, 0 } };
    for (int g = 0; g < v11->ngroups; g++) {
        for (int i = 0; i < v11->groups[g].nvertices; i++) {
            v11->groups[g].vertices[i].n = v3;
        }
    }
    br_vector3 v0v1, v2v1, normal;
    for (int g = 0; g < v11->ngroups; g++) {
        v11group* group = &v11->groups[g];
        for (int i = 0; i < group->nfaces; i++) {
            v11face* f = &group->faces[i];
            fmt_vertex* v0 = &group->vertices[f->vertices[0]];
            fmt_vertex* v1 = &group->vertices[f->vertices[1]];
            fmt_vertex* v2 = &group->vertices[f->vertices[2]];
            BrVector3Sub(&v0v1, &v0->p, &v1->p);
            BrVector3Sub(&v2v1, &v2->p, &v1->p);
            BrVector3Cross(&normal, &v0v1, &v2v1);
            BrVector3Accumulate(&v0->n, &normal);
            BrVector3Accumulate(&v1->n, &normal);
            BrVector3Accumulate(&v2->n, &normal);
        }
    }
    for (int g = 0; g < v11->ngroups; g++) {
        for (int i = 0; i < v11->groups[g].nvertices; i++) {
            BrVector3Normalise(&v11->groups[g].vertices[i].n, &v11->groups[g].vertices[i].n);
        }
    }

    /* Reused scratch space: models are rebuilt every frame for shadows. */
    static gl_vertex* verts;
    static unsigned int* indices;
    static int verts_capacity, indices_capacity;
    if (total_verts > verts_capacity) {
        gl_vertex* grown = realloc(verts, sizeof(gl_vertex) * total_verts);
        if (grown == NULL) LOG_PANIC("out of memory buffering model");
        verts = grown;
        verts_capacity = total_verts;
    }
    if (3 * total_faces > indices_capacity) {
        unsigned int* grown = realloc(indices, sizeof(int) * 3 * total_faces);
        if (grown == NULL) LOG_PANIC("out of memory buffering model");
        indices = grown;
        indices_capacity = 3 * total_faces;
    }

    ctx->bounds_valid = total_verts > 0;
    for (int a = 0; a < 3; ++a) {
        ctx->bounds_min[a] = FLT_MAX;
        ctx->bounds_max[a] = -FLT_MAX;
    }
    int v_index = 0;
    int i_index = 0;
    int face_offset = 0;
    for (int g = 0; g < v11->ngroups; g++) {
        for (int i = 0; i < v11->groups[g].nvertices; i++) {
            fmt_vertex* v = &v11->groups[g].vertices[i];
            verts[v_index].p = v->p;
            /* Use the exact uploaded vertices, including deformations/pivots. */
            for (int a = 0; a < 3; ++a) {
                float value = v->p.v[a];
                if (!isfinite(value)) ctx->bounds_valid = 0;
                if (value < ctx->bounds_min[a]) ctx->bounds_min[a] = value;
                if (value > ctx->bounds_max[a]) ctx->bounds_max[a] = value;
            }
            verts[v_index].n = v->n;
            verts[v_index].map = v->map;
            verts[v_index].colour_index = BR_ALPHA(v11->groups[g].vertex_colours[i]);
            v_index++;
        }
        for (int i = 0; i < v11->groups[g].nfaces; i++) {
            v11face* f = &v11->groups[g].faces[i];
            indices[i_index++] = f->vertices[0] + face_offset;
            indices[i_index++] = f->vertices[1] + face_offset;
            indices[i_index++] = f->vertices[2] + face_offset;
        }
        face_offset += v11->groups[g].nvertices;
    }

    if (!DRTakePooledModelBuffers(&ctx->vao_id, &ctx->vbo_id, &ctx->ebo_id)) {
        glGenVertexArrays(1, &ctx->vao_id);
        glGenBuffers(1, &ctx->vbo_id);
        glGenBuffers(1, &ctx->ebo_id);
    }

    // Vertices
    glBindVertexArray(ctx->vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->vbo_id);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex) * total_verts, verts, GL_STATIC_DRAW);
    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, p));
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, n));
    glEnableVertexAttribArray(1);
    // uv coordinates
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, map));
    glEnableVertexAttribArray(2);
    // color
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), (void*)offsetof(gl_vertex, colour_index));
    glEnableVertexAttribArray(3);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo_id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * 3 * total_faces, indices, GL_STATIC_DRAW);
    glBindVertexArray(0);

    model->stored = ctx;

    CHECK_GL_ERROR("after build model");
}

void setActiveMaterial(tStored_material* material) {
    if (material == NULL || material == current_material) {
        return;
    }

    DRProfileCount(DRP_MATERIALS,1);
    glUniformMatrix2x3fv(uniforms_3d.material_uv_transform, 1, GL_TRUE, &material->map_transform.m[0][0]);

    if (material->pixelmap) {
        tStored_pixelmap* stored_px = EnsureTexture(material->pixelmap);
        if (stored_px == NULL) {
            LOG_PANIC("stored_px is null for pixelmap %s", material->pixelmap->identifier);
        }
        if (stored_px->mips_need_palette && mip_palette_valid) {
            UploadIndexedTexture(material->pixelmap, stored_px);
        }
        glBindTexture(GL_TEXTURE_2D, stored_px->id);
        glUniform1ui(uniforms_3d.material_texture_enabled, 1);
    } else {
        glUniform1ui(uniforms_3d.material_texture_enabled, 0);

        // index_base and index_range are only used for untextured materials
        glUniform1ui(uniforms_3d.material_index_base, material->index_base);
        // glUniform1ui(uniforms_3d.material_index_range, material->index_range);  TODO: re-add when we support untextured lighting
    }

    if (material->shade_table) {
        glUniform1ui(uniforms_3d.material_shade_table_height, material->shade_table->height);
        GLRenderer_SetShadeTable(material->shade_table);
    }

    if (material->index_blend) {
        glUniform1ui(uniforms_3d.material_blend_enabled, 1);
        GLRenderer_SetBlendTable(material->index_blend);
        // materials with index_blend do not write to depth buffer (https://www.cwaboard.co.uk/viewtopic.php?p=105846&sid=58ad8910238000ca14b01dad85117175#p105846)
        glDepthMask(GL_FALSE);
    } else {
        glUniform1ui(uniforms_3d.material_blend_enabled, 0);
        glDepthMask(GL_TRUE);
    }

    glUniform1ui(uniforms_3d.material_flags, material->flags);

    if (material->flags & (BR_MATF_TWO_SIDED | BR_MATF_ALWAYS_VISIBLE)) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }

    CHECK_GL_ERROR("GLRenderer_RenderModelxx");
}

void GLRenderer_Model(br_actor* actor, br_model* model, br_material* material, br_token render_type, br_matrix34 model_matrix) {
    tStored_model_context* ctx;
    v11group* group;
    int element_index = 0;

    uint64_t profile_model=DRProfileClock();
    DRProfileCount(DRP_MODELS,1);
    if (model->flags & BR_MODF_DETHRACE_FORCE_BUFFER_UPDATE) {
        if (model->stored) {
            ((br_object*)model->stored)->dispatch->_free((br_object*)model->stored);
            model->stored = NULL;
        }
        GLRenderer_BufferModel(model);
        model->flags &= ~BR_MODF_DETHRACE_FORCE_BUFFER_UPDATE;
    }

    ctx = model->stored;
    v11model* v11 = model->prepared;

    if (v11 == NULL) {
        // LOG_WARN("No model prepared for %s", model->identifier);
        DRProfileTime(DRP_MODEL,profile_model);
        return;
    }

#ifdef __ANDROID__
    if (ctx && ctx->bounds_valid && !DRBoundsVisible(ctx->bounds_min, ctx->bounds_max, model_matrix.m, frustum_planes)) {
        DRProfileCount(DRP_CULLED,1);
        DRProfileTime(DRP_MODEL,profile_model);
        return;
    }
#endif

    // model matrix
    br_matrix4 view_model_matrix, model_matrix4, inverse, inverse_transpose;
    BrMatrix4Copy34(&model_matrix4, &model_matrix);
    glUniformMatrix4fv(uniforms_3d.model, 1, GL_FALSE, model_matrix4.m[0]);

    /* Only the environment-mapping shader branch uses the normal matrix.
     * Ordinary track materials need no per-model inverse/transpose. */
    int needs_normal = !material->stored || (((tStored_material*)material->stored)->flags & BR_MATF_ENVIRONMENT_L);
    for (int g = 0; g < v11->ngroups && !needs_normal; ++g)
        needs_normal = v11->groups[g].stored && (((tStored_material*)v11->groups[g].stored)->flags & BR_MATF_ENVIRONMENT_L);
    if (needs_normal) {
        // normal matrix (http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/)
        BrMatrix4Mul(&view_model_matrix, &view_matrix, &model_matrix4);
        BrMatrix4Inverse(&inverse, &view_model_matrix);
        // transpose
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                inverse_transpose.m[i][j] = inverse.m[j][i];
            }
        }
        glUniformMatrix4fv(uniforms_3d.normal_matrix, 1, GL_FALSE, &inverse_transpose.m[0][0]);
    }

    glBindVertexArray(ctx->vao_id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->ebo_id);

    // set default material for this actor/model
    setActiveMaterial(material->stored);

    switch (render_type) {
    case BRT_TRIANGLE:
        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    case BRT_LINE:
        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform1ui(uniforms_3d.material_index_base, 255);
        glUniform1ui(uniforms_3d.material_flags, 0);
        break;
    default:
        LOG_PANIC("render_type %d is not supported?!", render_type);
    }

    for (int g = 0; g < v11->ngroups; g++) {
        group = &v11->groups[g];
        setActiveMaterial(group->stored);
        DRProfileCount(DRP_DRAWS,1);
        DRProfileCount(DRP_TRIANGLES,group->nfaces);
        glDrawElements(GL_TRIANGLES, group->nfaces * 3, GL_UNSIGNED_INT, (void*)(element_index * sizeof(int)));
        element_index += group->nfaces * 3;
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    dirty_buffers = 1;

    CHECK_GL_ERROR("GLRenderer_RenderModel");
    DRProfileTime(DRP_MODEL,profile_model);
}

void GLRenderer_BufferMaterial(br_material* mat) {
    tStored_material* stored = mat->stored;
    if (!stored) {
        stored = NewStoredMaterial();
        mat->stored = stored;
        if (mat->identifier) {
            strcpy(stored->identifier, mat->identifier);
        }
    }
    BrMatrix23Copy(&stored->map_transform, &mat->map_transform);
    stored->pixelmap = mat->colour_map;
    stored->flags = mat->flags;
    stored->shade_table = mat->index_shade;
    stored->index_base = mat->index_base;
    stored->index_range = mat->index_range;
    stored->index_blend = mat->index_blend;
}

/* Upload a material pixelmap as an R8UI texture. With Smooth texture
 * filtering a full indexed mip chain is added (see texture_mips.h) so the 3D
 * shader can pick a level for distant/oblique surfaces instead of skipping
 * texels, which is what makes roads and fences shimmer. Sharp keeps the
 * original single-level, nearest texture. */
static void UploadIndexedTexture(br_pixelmap* pm, tStored_pixelmap* stored) {
    // sometimes the pixelmap has row_bytes > width. OpenGL expects linear pixels, so flatten it out
    uint8_t* linear_pixels = malloc(sizeof(uint8_t) * pm->width * pm->height);
    uint8_t* original_pixels = pm->pixels;
    for (int y = 0; y < pm->height; y++) {
        for (int x = 0; x < pm->width; x++) {
            linear_pixels[y * pm->width + x] = original_pixels[y * pm->row_bytes + x];
        }
    }

    glBindTexture(GL_TEXTURE_2D, stored->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, pm->width, pm->height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, linear_pixels);

    int levels = texture_filtering ? DRMipLevelCount(pm->width, pm->height) : 1;
    if (levels > 1) {
        const uint8_t* palette = mip_palette_valid ? mip_palette : NULL;
        int w = pm->width, h = pm->height;
        uint8_t* mip = malloc(sizeof(uint8_t) * (w / 2 + 1) * (h / 2 + 1));
        for (int level = 1; level < levels; level++) {
            w = w > 1 ? w / 2 : 1;
            h = h > 1 ? h / 2 : 1;
            DRBuildIndexedMip(mip, w, h, linear_pixels, pm->width, pm->height, palette);
            glTexImage2D(GL_TEXTURE_2D, level, GL_R8UI, w, h, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, mip);
        }
        free(mip);
    }
    /* Integer textures are only complete with NEAREST(_MIPMAP_NEAREST); the
     * shader does its own palette-aware bilinear/trilinear on top. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
    stored->mips_need_palette = levels > 1 && !mip_palette_valid;
    if (stored->mips_need_palette) RememberPendingMips(pm);
    free(linear_pixels);
}

void GLRenderer_BufferTexture(br_pixelmap* pm) {
    tStored_pixelmap* stored = pm->stored;
    if (pm->stored) {
    } else {
        stored = NewStoredPixelmap();
        glGenTextures(1, &stored->id);
        pm->stored = stored;
    }

    UploadIndexedTexture(pm, stored);

    CHECK_GL_ERROR("GLRenderer_BufferTexture");
}

void GLRenderer_FreeTexture(br_pixelmap* pm) {
    tStored_pixelmap* stored = pm->stored;
    if (stored == NULL) return;
    ForgetPendingMips(pm);
    if (stored->id) glDeleteTextures(1, &stored->id);
    free(stored);
    pm->stored = NULL;
    if (current_shade_table == pm) current_shade_table = NULL;
    current_material = NULL;
    CHECK_GL_ERROR("GLRenderer_FreeTexture");
}

void GLRenderer_FreeMaterial(br_material* mat) {
    if (mat->stored == NULL) return;
    if (current_material == mat->stored) current_material = NULL;
    free(mat->stored);
    mat->stored = NULL;
}

/* A pixelmap whose buffer was released but which is still referenced (the
 * original free was a no-op, so old code paths may rely on that) is
 * uploaded again instead of crashing. */
static tStored_pixelmap* EnsureTexture(br_pixelmap* pm) {
    if (pm->stored == NULL && pm->pixels != NULL) {
        LOG_WARN("re-uploading released texture %s", pm->identifier ? pm->identifier : "(unnamed)");
        GLRenderer_BufferTexture(pm);
    }
    return pm->stored;
}

#ifdef __ANDROID__
/* Read only the pixels the CPU actually consumes. texelFetch preserves the
 * old exact x*factor/y*factor samples (including palette index zero). Keep
 * supersampled geometry on the GPU and restore state for mid-scene blends. */
static void ReadbackAtCanvasResolution(void) {
    /* Mirror scenes are drawn at 1x (see GLRenderer_BeginScene). */
    int factor = mirror_scene ? mirror_factor : super_factor;
    /* Only the drawn pixelmap's rectangle is merged back (FlushBuffer), so
     * read only that: the mirror is a small sub-rectangle of the canvas. */
    int rx = 0, ry = 0, rw = render_width, rh = render_height;
    if (last_colour_buffer != NULL) {
        rx = last_colour_buffer->base_x;
        ry = render_height - last_colour_buffer->base_y - last_colour_buffer->height;
        rw = last_colour_buffer->width;
        rh = last_colour_buffer->height;
        if (rx < 0) { rw += rx; rx = 0; }
        if (ry < 0) { rh += ry; ry = 0; }
        if (rx + rw > render_width) rw = render_width - rx;
        if (ry + rh > render_height) rh = render_height - ry;
        if (rw <= 0 || rh <= 0) return;
    }
    uint8_t* destination = screen_buffer_flip_pixels + ry * render_width + rx;
    glPixelStorei(GL_PACK_ROW_LENGTH, render_width);
    if (factor == 1 && !pending_shadow) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glReadPixels(rx, ry, rw, rh, GL_RED_INTEGER, GL_UNSIGNED_BYTE, destination);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        return;
    }
    GLint program, vao, active_texture, texture, shadow_binding, viewport[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &shadow_binding);
    glBindTexture(GL_TEXTURE_2D, deferred_shadow_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cull = glIsEnabled(GL_CULL_FACE);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, readback_framebuffer);
    /* The quad covers the viewport only; gl_FragCoord stays absolute. */
    glViewport(rx, ry, rw, rh);
    glUseProgram(readback_program);
    glUniform1i(readback_sample_factor, factor);
    glUniform1i(readback_shadow_pixels, 1);
    glUniform1i(readback_use_shadow, pending_shadow);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
    glBindVertexArray(screen_buffer_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glReadPixels(rx, ry, rw, rh, GL_RED_INTEGER, GL_UNSIGNED_BYTE, destination);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glBindVertexArray(vao);
    glBindTexture(GL_TEXTURE_2D, texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadow_binding);
    glActiveTexture(active_texture);
    glUseProgram(program);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (cull) glEnable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
}
#endif

void GLRenderer_FlushBuffer(tRenderer_flush_type flush_type) {
    if (!dirty_buffers
#ifdef __ANDROID__
        && !pending_shadow
#endif
    ) {
        return;
    }
    uint8_t* pm_pixels = last_colour_buffer->pixels;
    uint16_t* depth_pixels = last_depth_buffer->pixels;

    int read_width = super_width, read_factor = super_factor;
#ifdef __ANDROID__
    uint64_t profile_readback = DRProfileClock();
    ReadbackAtCanvasResolution();
    DRProfileTime(DRP_READBACK,profile_readback);
    pending_shadow = 0;
    DRProfileCount(DRP_READS,1);
    DRProfileCount(DRP_BYTES,(uint64_t)render_width*render_height);
    read_width = render_width; read_factor = 1;
#else
    // pull super-res framebuffer into cpu memory; we downsample below for the
    // pixmap and keep the GPU texture itself for the high-res present path.
    if (opengl_profile == eOpenGL_profile_es) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glReadPixels(0, 0, super_width, super_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, screen_buffer_flip_pixels);
    } else {
        glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, screen_buffer_flip_pixels);
    }

#endif
    CHECK_GL_ERROR("GLRenderer_FlushBuffer3");
    uint64_t profile_copy = DRProfileClock();

    // flip Y and nearest-neighbour downsample super → pixmap. last_colour_buffer
    // can be a sub-pixmap of gBack_screen (e.g. the wreck gallery uses
    // BrPixelmapAllocateSub), so iterate its width/height and apply its
    // base_x/base_y to the framebuffer source coords. Writing the full pixmap
    // size into the sub-pixmap's pointer would overflow into adjacent rows
    // and produce ghost copies of the 3D scene elsewhere on the back buffer.
    //
    // BRender sub-pixmaps share the parent's `pixels` pointer and encode their
    // offset only in base_x/base_y (see _M_br_device_pixelmap_mem_allocateSub;
    // DethRace's gRender_screen also resets `pixels` to gBack_screen->pixels
    // every frame). The destination must therefore apply base_x/base_y too,
    // like every CPU pixelmap op and DepthEffect do. Without it, any non-zero
    // render offset (ScreenSize/gRender_indent > 0 from OPTIONS.TXT or the
    // screen-smaller key, the map-mode inset, cockpit render_top) lands the 3D
    // scene in the top-left corner while the CPU sky/borders stay centred.
    {
        pm_pixels += last_colour_buffer->base_y * last_colour_buffer->row_bytes + last_colour_buffer->base_x;
        int dest_y = last_colour_buffer->height;
        int src_y_pix = render_height - last_colour_buffer->base_y - last_colour_buffer->height;
        uint8_t new_pixel;
        for (int y = 0; y < last_colour_buffer->height; y++) {
            dest_y--;
            int src_super_y = src_y_pix * read_factor;
            if (read_factor == 1) {
                DRMergeIndexedRow(pm_pixels + dest_y * last_colour_buffer->row_bytes,
                    screen_buffer_flip_pixels + src_super_y * read_width + last_colour_buffer->base_x,
                    last_colour_buffer->width);
            } else {
                for (int x = 0; x < last_colour_buffer->width; x++) {
                    int src_super_x = (last_colour_buffer->base_x + x) * read_factor;
                    new_pixel = screen_buffer_flip_pixels[src_super_y * read_width + src_super_x];
                    if (new_pixel != 0) {
                        pm_pixels[dest_y * last_colour_buffer->row_bytes + x] = new_pixel;
                    }
                }
            }
            src_y_pix++;
        }
    }

    // Snapshot the back buffer state right after the 3D readback. The present
    // shader compares this to the final back buffer to tell which pixels were
    // touched by 2D draws that ran after the 3D pass. We always copy from
    // gBack_screen (not from last_colour_buffer, which may be a sub-pixmap)
    // because the present shader sees the full back buffer.
    if (snapshot_pixels != NULL
#ifdef __ANDROID__
        && !mirror_scene
#endif
    ) {
        extern br_pixelmap* gBack_screen;
        if (gBack_screen != NULL && gBack_screen->pixels != NULL) {
            memcpy(snapshot_pixels, gBack_screen->pixels, render_width * render_height);
            snapshot_dirty = snapshot_nonzero = 1;
        }
    }

    CHECK_GL_ERROR("GLRenderer_FlushBuffer2");

    if (flush_type == eFlush_all) {

        // pull depth buffer into cpu memory to emulate BRender behavior. Read
        // at super-res and nearest-neighbour downsample like the colour pass.
#ifdef __ANDROID__
        // GLES does not allow glReadPixels with GL_DEPTH_COMPONENT.
        // Skipping disables depth-dependent CPU effects (shadows, etc.) but keeps the renderer alive.
        for (int y = 0; y < last_depth_buffer->height; ++y)
            memset((uint8_t*)depth_pixels + y * last_depth_buffer->row_bytes, 0xff, last_depth_buffer->width * sizeof(uint16_t));
#else
        if (opengl_profile == eOpenGL_profile_es) {
            glReadPixels(0, 0, super_width, super_height, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depth_buffer_flip_pixels);
        } else {
            glBindTexture(GL_TEXTURE_2D, depth_texture);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, depth_buffer_flip_pixels);
        }

        int dest_y = last_colour_buffer->height;
        int src_y = render_height - last_colour_buffer->base_y - last_colour_buffer->height;

        for (int y = 0; y < last_colour_buffer->height; y++) {
            dest_y--;
            int src_super_y = src_y * super_factor;
            for (int x = 0; x < last_colour_buffer->width; x++) {
                int src_super_x = (last_colour_buffer->base_x + x) * super_factor;
                uint16_t new_depth = depth_buffer_flip_pixels[src_super_y * super_width + src_super_x];
                depth_pixels[dest_y * render_width + x] = new_depth;
            }
            src_y++;
        }
#endif
    }
    {
        // Same integer-clear story as in GLRenderer_ClearBuffers — glClear on
        // a R8UI colour attachment is undefined on GLES.
        static const GLuint zero[4] = { 0, 0, 0, 0 };
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glClearBufferuiv(GL_COLOR, 0, zero);
    }
    DRProfileTime(DRP_COPY,profile_copy);
    flush_counter++;
    dirty_buffers = 0;
    CHECK_GL_ERROR("GLRenderer_FlushBuffer");
}

void GLRenderer_FlushBuffers(void) {
#ifdef __ANDROID__
    if (shadow_batch) return;
#endif
    GLRenderer_FlushBuffer(eFlush_all);
}

#ifdef __ANDROID__
void DRBeginShadowBatch(void) {
    /* Flush any earlier work before entering this same-target GPU-only batch. */
    GLRenderer_FlushBuffers();
    shadow_batch = 1;
}
/* Same render target is retained until the following main-scene CPU boundary.
 * Preserve a separate layer so blend snapshots see the same scene as before. */
void DRDeferShadowBatch(void) {
    shadow_batch = 0;
    if (!dirty_buffers) return;
    GLint active, binding;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glBindTexture(GL_TEXTURE_2D, deferred_shadow_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, super_width, super_height);
    glBindTexture(GL_TEXTURE_2D, binding);
    glActiveTexture(active);
    static const GLuint zero[4] = {0,0,0,0};
    glClearBufferuiv(GL_COLOR, 0, zero);
    pending_shadow = 1;
    dirty_buffers = 0;
    /* This is a logical scene boundary even though CPU synchronization waits. */
    flush_counter++;
    CHECK_GL_ERROR("defer shadow readback");
}
void DREndShadowBatch(void) {
    shadow_batch = 0;
    GLRenderer_FlushBuffers();
}
#endif

void GLRenderer_SetViewport(int x, int y, int width, int height) {
    vp_x = x;
    vp_y = y;
    vp_width = width;
    vp_height = height;
}
