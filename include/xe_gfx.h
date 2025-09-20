#ifndef XE_GFX_H
#define XE_GFX_H

#include <llulu/lu_math.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * TODO:
 * Pipelines as resource (like images)
 */

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

enum xe_gfx_op_flags {
    XE_OP_DEPTH_TEST   = 0x0001,
    XE_OP_STENCIL_TEST = 0x0002,
    XE_OP_DEPTH_MASK   = 0x0004,
    XE_OP_STENCIL_MASK = 0x0008,
    XE_OP_BLEND        = 0x0010,
    XE_OP_CULL         = 0x0020,
    XE_OP_SCISSOR      = 0x0040,
    XE_OP_MULTISAMPLE  = 0x0080
};

enum xe_gfx_blend_func {
    XE_BLEND_NOOP,
    XE_BLEND_ONE,
    XE_BLEND_SRC_ALPHA,
    XE_BLEND_ONE_MINUS_SRC_ALPHA,
    XE_BLEND_ZERO,
    XE_BLEND_COUNT
};

enum xe_gfx_cull_face {
    XE_CULL_NOOP,
    XE_CULL_FRONT,
    XE_CULL_BACK,
    XE_CULL_FRONT_AND_BACK,
    XE_CULL_COUNT
};

enum xe_gfx_depth_func {
    XE_DEPTH_NOOP,
    XE_DEPTH_LEQUAL,
    XE_DEPTH_LESS,
    XE_DEPTH_COUNT
};

enum xe_gfx_clear_flags {
    XE_CLEAR_COLOR = 0x01,
    XE_CLEAR_DEPTH = 0x02,
    XE_CLEAR_STENCIL = 0x04
};

typedef struct xe_gfx_rops {
    uint32_t enabled_flags;
    uint8_t blend_src_fn;
    uint8_t blend_dst_fn;
    uint8_t depth_fn;
    uint8_t cull_faces;
    lu_rect clip;
} xe_gfx_rops;

typedef struct xe_gfx_draw_batch {
    uint16_t start_offset; /* in draw units */
    uint16_t batch_size;   /* in draw units */
    xe_gfx_rops rops;
} xe_gfx_draw_batch;

typedef struct xe_gfx_renderpass_desc {
    
} xe_gfx_renderpass_desc;

typedef struct xe_gfx_renderpass {
    /* TODO: Framebuffer */
    lu_rect viewport;
    float background_color[3];
    uint16_t clear_flags;
    uint16_t head;
    xe_gfx_draw_batch batches[1024];
} xe_gfx_renderpass;

typedef struct xe_gfx_config {
    void *(*gl_loader)(const char *);
    const char *vert_shader_path;
    const char *frag_shader_path;
    xe_gfx_rops default_ops;
} xe_gfx_config;

bool xe_gfx_init(xe_gfx_config *config);

/* This function should be called every frame before writing data to any persistent coherent buffer. */
void xe_gfx_sync(void);

/* Flushes, unmaps and deletes gpu resources. It is up to the programmer to call or skip this function. */
void xe_gfx_shutdown(void);

xe_gfx_tex xe_gfx_tex_alloc(xe_gfx_texfmt format);
void xe_gfx_tex_load(xe_gfx_tex tex, void *data);

/* Adds  */
void xe_gfx_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, xe_gfx_material *material);
void xe_gfx_render(int viewport_width, int viewport_height);

#endif /* XE_GFX_H */

