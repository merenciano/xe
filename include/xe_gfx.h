#ifndef XE_GFX_H
#define XE_GFX_H

#include <llulu/lu_math.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * TODO:
 * Pipelines as resource (like images)
 */


/* Configurable: uint16_t or uint32_t */
typedef uint16_t xe_gfx_idx;

typedef struct xe_gfx_vtx {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} xe_gfx_vtx;

enum xe_gfx_tex_pixel_format {
    XE_TEX_R = 0,
    XE_TEX_RG,
    XE_TEX_RGB,
    XE_TEX_SRGB,
    XE_TEX_RGBA,
    XE_TEX_R_F16,
    XE_TEX_RG_F16,
    XE_TEX_RGB_F16,
    XE_TEX_RGBA_F16,
    XE_TEX_R_F32,
    XE_TEX_RG_F32,
    XE_TEX_RGB_F32,
    XE_TEX_RGBA_F32,
    XE_TEX_FMT_COUNT
};

typedef struct xe_gfx_texfmt {
    uint16_t width;
    uint16_t height;
    uint16_t format; /* see: enum xe_gfx_tex_pixel_format */
    uint16_t flags; /* unused */
} xe_gfx_texfmt;

typedef struct xe_gfx_tex {
    int idx;
    int layer;
} xe_gfx_tex;

typedef struct xe_gfx_material {
    lu_mat4 model;
    lu_vec4 color; // TODO: Remove since it's in the vertex already
    lu_vec4 darkcolor;
    xe_gfx_tex tex;
    float pma;
} xe_gfx_material;

enum xe_gfx_blend_func {
    XE_BLEND_UNSET,
    XE_BLEND_DISABLED,
    XE_BLEND_ONE,
    XE_BLEND_SRC_ALPHA,
    XE_BLEND_ONE_MINUS_SRC_ALPHA,
    XE_BLEND_ZERO,
    XE_BLEND_COUNT
};

enum xe_gfx_cull_face {
    XE_CULL_UNSET,
    XE_CULL_NONE,
    XE_CULL_FRONT,
    XE_CULL_BACK,
    XE_CULL_FRONT_AND_BACK,
    XE_CULL_COUNT
};

enum xe_gfx_depth_func {
    XE_DEPTH_UNSET,
    XE_DEPTH_DISABLED,
    XE_DEPTH_LEQUAL,
    XE_DEPTH_LESS,
    XE_DEPTH_COUNT
};

typedef struct xe_gfx_rops {
    lu_rect clip;
    uint8_t blend_src;
    uint8_t blend_dst;
    uint8_t depth;
    uint8_t cull;
} xe_gfx_rops;

enum xe_gfx_rops_default_type_flags {
    XE_ROPS_DEFAULT_DEPTH = 0x01, /* Enable depth test with less func */
    XE_ROPS_DEFAULT_BLEND = 0x02, /* Enable blend mix (one, one_minus_src_alpha) */
    XE_ROPS_DEFAULT_CULL  = 0x04, /* Enable backface culling */
};

static inline xe_gfx_rops
xe_gfx_rops_default(int flags) /* XE_ROPS_DEFAULT_..., see: enum xe_gfx_rops_default_type_flags */ 
{
    return (xe_gfx_rops){
        .clip = {0, 0, 0, 0},
        .blend_src = flags & XE_ROPS_DEFAULT_BLEND ?
            XE_BLEND_ONE : XE_BLEND_DISABLED,
        .blend_dst = flags & XE_ROPS_DEFAULT_BLEND ?
            XE_BLEND_ONE_MINUS_SRC_ALPHA : XE_BLEND_DISABLED,
        .depth = flags & XE_ROPS_DEFAULT_DEPTH ?
            XE_DEPTH_LESS : XE_DEPTH_DISABLED,
        .cull = flags & XE_ROPS_DEFAULT_CULL ?
            XE_CULL_BACK : XE_CULL_NONE
    };
}

typedef struct xe_gfx_draw_batch {
    ptrdiff_t start_offset; /* in draw units */
    int batch_size;   /* in draw units */
    xe_gfx_rops rops;
} xe_gfx_draw_batch;

typedef struct xe_gfx_renderpass {
    /* TODO: Framebuffer */
    lu_rect viewport;
    lu_color bg_color;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    int head;
    xe_gfx_draw_batch batches[512];
} xe_gfx_renderpass;

typedef struct xe_gfx_config {
    void *(*gl_loader)(const char *);
    const char *vert_shader_path;
    const char *frag_shader_path;
    xe_gfx_rops default_ops;
    lu_color background_color;
    lu_rect viewport;
} xe_gfx_config;

bool xe_gfx_init(xe_gfx_config *config);

/* This function should be called every frame before writing data to any persistent coherent buffer. */
void xe_gfx_sync(void);

/* Flushes, unmaps and deletes gpu resources. It is up to the programmer to call or skip this function. */
void xe_gfx_shutdown(void);

xe_gfx_tex xe_gfx_tex_alloc(xe_gfx_texfmt format);
void xe_gfx_tex_load(xe_gfx_tex tex, const void *data);

/* Adds  */
void xe_gfx_pass_begin(lu_rect viewport, lu_color background,
                       bool clear_color, bool clear_depth, bool clear_stencil,
                       xe_gfx_rops ops);

void xe_gfx_rops_set(xe_gfx_rops ops);
void xe_gfx_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, const xe_gfx_material *material);
void xe_gfx_render(void);

#endif /* XE_GFX_H */

