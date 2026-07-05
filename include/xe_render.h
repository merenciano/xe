#ifndef XE_RENDER_H
#define XE_RENDER_H

#include <llulu/lu_math.h>
#include <stdint.h>
#include <stdbool.h>

enum {
    XE_SHADER_DATA_BYTES = 256,
    XE_PROGRAM_UNSET = 0,
};

typedef uint16_t xe_vtx_idx; /* Configurable: uint16_t or uint32_t */
typedef uint32_t xe_program;

typedef struct xe_vtx {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} xe_vtx;

enum xe_tex_pixfmt {
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

typedef struct xe_texfmt {
    uint16_t width;
    uint16_t height;
    uint16_t format; /* see: enum xe_tex_pixfmt */
    uint16_t flags; /* unused */
} xe_texfmt;

typedef struct xe_tex {
    int idx;
    int layer;
} xe_tex;

struct xe_shader_generic_spine_data {
    lu_mat4 model;
    lu_vec4 color; // TODO: Remove since it's in the vertex already
    lu_vec4 darkcolor; // darkColor.a == PMA
    int32_t albedo_idx;
    float albedo_layer;
    float pma; // TODO: Use DarkColor.a ???
    float padding;
};

typedef union xe_shader_data {
    uint8_t buf[XE_SHADER_DATA_BYTES];
    struct xe_shader_generic_spine_data generic;
} xe_shader_data;

typedef struct xe_material {
    xe_shader_data data;
    xe_program program;
} xe_material;

typedef struct xe_shader_sources {
    const char *vert_src;
    size_t vert_len;
    const char *frag_src;
    size_t frag_len;
} xe_shader_sources;

enum xe_blend_func {
    XE_BLEND_UNSET,
    XE_BLEND_DISABLED,
    XE_BLEND_ONE,
    XE_BLEND_SRC_ALPHA,
    XE_BLEND_ONE_MINUS_SRC_ALPHA,
    XE_BLEND_ZERO,
    XE_BLEND_COUNT
};

enum xe_cull_face {
    XE_CULL_UNSET,
    XE_CULL_NONE,
    XE_CULL_FRONT,
    XE_CULL_BACK,
    XE_CULL_FRONT_AND_BACK,
    XE_CULL_COUNT
};

enum xe_depth_func {
    XE_DEPTH_UNSET,
    XE_DEPTH_DISABLED,
    XE_DEPTH_LEQUAL,
    XE_DEPTH_LESS,
    XE_DEPTH_COUNT
};

typedef struct xe_draw_state {
    lu_rect clip;
    uint8_t blend_src;
    uint8_t blend_dst;
    uint8_t depth;
    uint8_t cull;
    xe_program pipeline;
} xe_draw_state;

enum xe_draw_state_default_type_flags {
    XE_DRAW_STATE_DEFAULT_DEPTH = 0x01, /* Enable depth test with less func */
    XE_DRAW_STATE_DEFAULT_BLEND = 0x02, /* Enable blend mix (one, one_minus_src_alpha) */
    XE_DRAW_STATE_DEFAULT_CULL  = 0x04, /* Enable backface culling */
};

static inline xe_draw_state
xe_draw_state_default(int flags) /* XE_DRAW_STATE_DEFAULT_..., see: enum xe_draw_state_default_type_flags */ 
{
    return (xe_draw_state){
        .clip = {0, 0, 0, 0},
        .blend_src = flags & XE_DRAW_STATE_DEFAULT_BLEND ?
            XE_BLEND_ONE : XE_BLEND_DISABLED,
        .blend_dst = flags & XE_DRAW_STATE_DEFAULT_BLEND ?
            XE_BLEND_ONE_MINUS_SRC_ALPHA : XE_BLEND_DISABLED,
        .depth = flags & XE_DRAW_STATE_DEFAULT_DEPTH ?
            XE_DEPTH_LESS : XE_DEPTH_DISABLED,
        .cull = flags & XE_DRAW_STATE_DEFAULT_CULL ?
            XE_CULL_BACK : XE_CULL_NONE,
        .pipeline = XE_PROGRAM_UNSET
    };
}

typedef struct xe_draw_batch {
    ptrdiff_t start_offset; /* in draw units */
    int batch_size;   /* in draw units */
    xe_draw_state state;
} xe_draw_batch;

typedef struct xe_renderpass {
    /* TODO: Framebuffer */
    lu_rect viewport;
    lu_color bg_color;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    int head;
    xe_draw_batch batches[512];
} xe_renderpass;

typedef struct xe_renderconf {
    void *(*gl_loader)(const char *);
    const char *vert_shader_path;
    const char *frag_shader_path;
    xe_draw_state default_ops;
    lu_color background_color;
    lu_rect viewport;
} xe_renderconf;

bool xe_render_init(xe_renderconf *config);

/* This function should be called every frame before writing data to any persistent coherent buffer. */
void xe_render_sync(void);

/* Flushes, unmaps and deletes gpu resources. It is up to the programmer to call or skip this function. */
void xe_render_shutdown(void);

xe_tex xe_render_tex_alloc(xe_texfmt format);
void xe_render_tex_load(xe_tex tex, const void *data);

xe_program xe_render_pipeline_alloc(void);
bool xe_render_pipeline_compile(xe_program pipeline, xe_shader_sources src);
void xe_render_pipeline_use(xe_program pipeline);

void xe_render_pass_begin(lu_rect viewport, lu_color background,
                       bool clear_color, bool clear_depth, bool clear_stencil,
                       xe_draw_state state);
void xe_render_draw_state_set(xe_draw_state state);
void xe_render_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, const xe_material *material);
void xe_render_draw(void);

#endif /* XE_RENDER_H */
