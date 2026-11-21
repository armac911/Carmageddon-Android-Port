#include "stored_context.h"
#include "../include/harness/trace.h"
#include <stdlib.h>

/* Models such as car shadows (every frame) and crash-deformed cars are
 * removed and rebuilt repeatedly. Keep their GL names for reuse instead of
 * deleting and regenerating them each time. */
#define DR_MODEL_GL_POOL 32
static struct { GLuint vao, vbo, ebo; } gl_pool[DR_MODEL_GL_POOL];
static int gl_pool_count;

int DRTakePooledModelBuffers(GLuint* vao, GLuint* vbo, GLuint* ebo) {
    if (gl_pool_count == 0) return 0;
    gl_pool_count--;
    *vao = gl_pool[gl_pool_count].vao;
    *vbo = gl_pool[gl_pool_count].vbo;
    *ebo = gl_pool[gl_pool_count].ebo;
    return 1;
}

void _free(br_object* o) {
    tStored_model_context* ctx = (tStored_model_context*)o;
    if (gl_pool_count < DR_MODEL_GL_POOL) {
        gl_pool[gl_pool_count].vao = ctx->vao_id;
        gl_pool[gl_pool_count].vbo = ctx->vbo_id;
        gl_pool[gl_pool_count].ebo = ctx->ebo_id;
        gl_pool_count++;
    } else {
        glDeleteVertexArrays(1, &ctx->vao_id);
        glDeleteBuffers(1, &ctx->vbo_id);
        glDeleteBuffers(1, &ctx->ebo_id);
    }
    free(o);
}

br_object_dispatch dispatch_funcs = {
    NULL,
    NULL,
    NULL,
    NULL,
    _free,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

tStored_model_context* NewStoredModelContext(void) {
    tStored_model_context* ctx = malloc(sizeof(tStored_model_context));
    ctx->dispatch = &dispatch_funcs;
    return ctx;
}

tStored_material* NewStoredMaterial(void) {
    tStored_material* ctx = malloc(sizeof(tStored_material));
    ctx->index_base = -1;
    ctx->pixelmap = NULL;
    ctx->shade_table = NULL;
    ctx->index_blend = NULL;
    ctx->identifier[0] = '\0';
    return ctx;
}

tStored_pixelmap* NewStoredPixelmap(void) {
    tStored_pixelmap* ctx = malloc(sizeof(tStored_pixelmap));
    ctx->id = 0;
    ctx->mips_need_palette = 0;
    return ctx;
}
