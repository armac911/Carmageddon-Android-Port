#include "gl_renderer.h"
#include "palette_filter.h"
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

static GLuint shader_program_2d;
static GLuint shader_program_3d;
#ifdef __ANDROID__
static GLuint readback_program, readback_framebuffer, readback_texture;
static GLuint deferred_shadow_texture;
static int pending_shadow;
#endif
static GLuint framebuffer_id, framebuffer_texture = 0;

// Pixmap snapshot taken right after the 3D readback inside FlushBuffer; the
// present-time shader compares the current pixmap to this snapshot to detect
// post-3D 2D draws (HUD/menu) so the rest can be drawn from the super-res
// 3D framebuffer instead of the downsampled pixmap.
static GLuint snapshot_texture = 0;
static uint8_t* snapshot_pixels = NULL;

// holds the latest uploaded version of the colour_buffer. Available in shader for blending
static GLuint current_colourbuffer_texture;

static uint8_t gl_palette[4 * 256]; // RGBA
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

struct {
    GLuint pixels, palette, snapshot, super_3d, smooth;
} uniforms_2d;

static GLuint CreateShaderProgram(char* name, const char* vertex_shader, const int vertex_shader_len, const char* fragment_shader, const int fragment_shader_len) {
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
    const GLchar* fragment_sources[] = { header->content, fragment_shader };
    const GLint fragment_source_lengths[] = { header->len, fragment_shader_len };
    glShaderSource(f_shader, 2, fragment_sources, fragment_source_lengths);
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
    glUniform1i(GetValidatedUniformLocation(readback_program, "sample_factor"), super_factor);
#endif
    shader_program_2d = CreateShaderProgram("framebuffer", vert->content, vert->len, frag->content, frag->len);
    glUseProgram(shader_program_2d);
    uniforms_2d.pixels = GetValidatedUniformLocation(shader_program_2d, "u_pixels");
    uniforms_2d.palette = GetValidatedUniformLocation(shader_program_2d, "u_palette");
    uniforms_2d.snapshot = GetValidatedUniformLocation(shader_program_2d, "u_snapshot");
    uniforms_2d.super_3d = GetValidatedUniformLocation(shader_program_2d, "u_super_3d");
    uniforms_2d.smooth = GetValidatedUniformLocation(shader_program_2d, "u_smooth");
    texture_filtering = 0;
#ifdef __ANDROID__
    const char* filtering = getenv("DETHRACE_IMAGE_FILTER");
    texture_filtering = filtering && strcmp(filtering, "smooth") == 0;
#endif
    // Final presentation stays nearest: filtering is now in the 3D material pass.
    glUniform1i(uniforms_2d.smooth, 0);

    // bind the uniform samplers to texture units:
    glUniform1i(uniforms_2d.pixels, 0);
    glUniform1i(uniforms_2d.palette, 1);
    glUniform1i(uniforms_2d.snapshot, 5);
    glUniform1i(uniforms_2d.super_3d, 6);

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
    screen_buffer_flip_pixels = malloc(sizeof(uint8_t) * super_width * super_height);
    depth_buffer_flip_pixels = malloc(sizeof(uint16_t) * super_width * super_height);
    snapshot_pixels = calloc(render_width * render_height, sizeof(uint8_t));

    CHECK_GL_ERROR("initializeOpenGLContext");
}

void GLRenderer_SetPalette(uint8_t* rgba_colors) {
    uint8_t previous_palette[sizeof(gl_palette)];
    memcpy(previous_palette, gl_palette, sizeof(gl_palette));
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

    if (texture_filtering && (!filter_lut_ready || memcmp(previous_palette, gl_palette, sizeof(gl_palette)) != 0)) {
        uint8_t lut[DR_PALETTE_LUT_SIZE];
        DRBuildPaletteLUT5(lut, gl_palette);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, filter_lut_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 128, GL_RED_INTEGER, GL_UNSIGNED_BYTE, lut);
        filter_lut_ready = 1;
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
    glActiveTexture(GL_TEXTURE2);
    tStored_pixelmap* stored = table->stored;
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
    glActiveTexture(GL_TEXTURE3);
    tStored_pixelmap* stored = table->stored;
    glBindTexture(GL_TEXTURE_2D, stored->id);

    // reset active texture back to default
    glActiveTexture(GL_TEXTURE0);
}

extern br_v1db_state v1db;

void GLRenderer_BeginScene(br_actor* camera, br_pixelmap* colour_buffer, br_pixelmap* depth_buffer) {
    last_colour_buffer = colour_buffer;
    last_depth_buffer = depth_buffer;
    /* The 3D pass renders into a super-res framebuffer, so scale viewport
     * dimensions and y-flip math by super_factor. */
    glViewport(colour_buffer->base_x * super_factor,
               (render_height - colour_buffer->height - colour_buffer->base_y) * super_factor,
               colour_buffer->width * super_factor,
               colour_buffer->height * super_factor);
    glUseProgram(shader_program_3d);

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
    //  switch back to default fb and reset state
    CHECK_GL_ERROR("GLRenderer_EndScene2");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthMask(GL_TRUE);
    glDepthMask(GL_TRUE);
    CHECK_GL_ERROR("GLRenderer_EndScene");
}

void GLRenderer_FullScreenQuad(uint8_t* screen_buffer) {
    uint64_t profile_present = DRProfileClock();

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
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, render_width, render_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, snapshot_pixels);

    // unit 6: super-res 3D framebuffer (R8UI), already on the GPU from the
    // 3D pass; no upload needed.
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);

    // restore active unit so subsequent draws aren't surprised
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(screen_buffer_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, screen_buffer_ebo);
    glUseProgram(shader_program_2d);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    CHECK_GL_ERROR("GLRenderer_RenderFullScreenQuad");
    DRProfileTime(DRP_PRESENT, profile_present);
}

void GLRenderer_ClearBuffers(void) {
#ifdef __ANDROID__
    pending_shadow = 0;
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
    if (snapshot_pixels != NULL) {
        memset(snapshot_pixels, 0, render_width * render_height);
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

    gl_vertex* verts = malloc(sizeof(gl_vertex) * total_verts);
    unsigned int* indices = malloc(sizeof(int) * 3 * total_faces);

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

    glGenVertexArrays(1, &ctx->vao_id);
    glGenBuffers(1, &ctx->vbo_id);
    glGenBuffers(1, &ctx->ebo_id);

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

    free(verts);
    free(indices);

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
        tStored_pixelmap* stored_px = material->pixelmap->stored;
        if (stored_px == NULL) {
            LOG_PANIC("stored_px is null for pixelmap %s", material->pixelmap->identifier);
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

void GLRenderer_BufferTexture(br_pixelmap* pm) {
    tStored_pixelmap* stored = pm->stored;
    if (pm->stored) {
    } else {
        stored = NewStoredPixelmap();
        glGenTextures(1, &stored->id);
        pm->stored = stored;
    }

    // sometimes the pixelmap has row_bytes > width. OpenGL expects linear pixels, so flatten it out
    uint8_t* linear_pixels = malloc(sizeof(uint8_t) * pm->width * pm->height);
    uint8_t* original_pixels = pm->pixels;
    for (int y = 0; y < pm->height; y++) {
        for (int x = 0; x < pm->width; x++) {
            linear_pixels[y * pm->width + x] = original_pixels[y * pm->row_bytes + x];
        }
    }

    glBindTexture(GL_TEXTURE_2D, stored->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, pm->width, pm->height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, linear_pixels);
    free(linear_pixels);

    CHECK_GL_ERROR("GLRenderer_BufferTexture");
}

#ifdef __ANDROID__
/* Read only the pixels the CPU actually consumes. texelFetch preserves the
 * old exact x*factor/y*factor samples (including palette index zero). Keep
 * supersampled geometry on the GPU and restore state for mid-scene blends. */
static void ReadbackAtCanvasResolution(void) {
    if (super_factor == 1 && !pending_shadow) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
        glReadPixels(0, 0, render_width, render_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, screen_buffer_flip_pixels);
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
    glViewport(0, 0, render_width, render_height);
    glUseProgram(readback_program);
    glUniform1i(glGetUniformLocation(readback_program, "shadow_pixels"), 1);
    glUniform1i(glGetUniformLocation(readback_program, "use_shadow_pixels"), pending_shadow);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);
    glBindVertexArray(screen_buffer_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glReadPixels(0, 0, render_width, render_height, GL_RED_INTEGER, GL_UNSIGNED_BYTE, screen_buffer_flip_pixels);
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
    {
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
    if (snapshot_pixels != NULL) {
        extern br_pixelmap* gBack_screen;
        if (gBack_screen != NULL && gBack_screen->pixels != NULL) {
            memcpy(snapshot_pixels, gBack_screen->pixels, render_width * render_height);
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
