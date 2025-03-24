#ifndef __XE_H__
#define __XE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
    TODO:
     -> Utils: Log, Alloc, Assert, Async jobs, Handles, Hash map, Pool (sparse)
     -> Profiling/Debug: Tracing 
     -> Fill the scene with more assets
     -> UBO to SSAO
     -> Node hierarchy (transform matrix)
     -> Spine dark color (color + darkcolor in shader data?)
     -> Blend modes (and depth, stencil, scissor, clip, viewport, clear, cull face)
     -> Text rendering
     -> Nuklear or cimgui integration

    Modules:
    - App
    - Platform
    - Scene (glTF 2.0)
        - glTF 2.0 import / export
        - Asset manager
        - Node graph: High lvl api (handles)
        - ECS: handle to internal representation and detailed API (lock-free async sched based on W/R of system updates)
    - Renderer OpenGL 4.6
*/

typedef struct xe_canvas {
    int w;
    int h;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    float bg_col[3];
} xe_canvas;

typedef struct xe_config {
    void *(*gl_loader)(const char *);
    xe_canvas canvas;
    float seconds_between_shader_file_changed_checks;
    const char *vert_shader_path;
    const char *frag_shader_path;
} xe_config;

typedef uint16_t xe_index;
typedef int xe_draw_idx;

enum { /* Texture flags */
    XE_TEX_DEPTH = 0x01,
    XE_TEX_STENCIL = 0x02,
    XE_TEX_NEAREST = 0x04, // filter linear if unset
    XE_TEX_REPEAT = 0x8, // clamp to edge if unset
    XE_TEX_MIRROR = 0x10, // if repeat: mirror
    XE_TEX_BORDER = 0x20, // if not repeat: clamp to border
};

enum { /* Pixel format */
    XE_PIXFMT_R = 0,
    XE_PIXFMT_RG,
    XE_PIXFMT_RGB,
    XE_PIXFMT_SRGB,
    XE_PIXFMT_RGBA,
    XE_PIXFMT_R_F16,
    XE_PIXFMT_RG_F16,
    XE_PIXFMT_RGB_F16,
    XE_PIXFMT_RGBA_F16,
    XE_PIXFMT_R_F32,
    XE_PIXFMT_RG_F32,
    XE_PIXFMT_RGB_F32,
    XE_PIXFMT_RGBA_F32,
    XE_PIXFMT_COUNT
};

typedef struct xe_texture_format {
    uint16_t width;
    uint16_t height;
    uint16_t pixel_fmt; /* see: XE_PIXFMT_ */
    uint16_t flags; /* see: XE_TEX_ */
} xe_texture_format;

typedef struct xe_texture {
    int idx;
    int layer;
} xe_texture;

typedef struct xe_image {
    const char *path;
    void *data;
    int w;
    int h;
    int channels;
    xe_texture tex;
} xe_image;

typedef struct xe_vertex {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} xe_vertex;

typedef struct xe_mesh {
    int base_vtx;
    int first_idx;
    int idx_count;
} xe_mesh;

typedef struct xe_node {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation;
    xe_mesh mesh;
    xe_texture tex;

#ifdef XE_DEBUG
    const char *name;
#endif
} xe_node;

typedef struct xe_material {
    /* absolute transform values */
    float apx;
    float apy;
    float asx;
    float asy;
    float arot;
    xe_texture tex;
} xe_material;

typedef struct xe_spine {
    xe_node node;
    struct spSkeleton *skel;
    struct spAnimationState *anim;
} xe_spine;

bool xe_init(xe_config *config);
xe_image xe_load_image(const char *path);
xe_texture xe_texture_reserve(xe_texture_format format);
void xe_texture_set(xe_texture tex, void *data);
xe_mesh xe_gfx_add_mesh(const void *vert, size_t vert_size, const void *indices, size_t indices_size);
xe_draw_idx xe_gfx_add_material(xe_material mat);
void xe_gfx_submit(xe_mesh mesh, xe_draw_idx drawidx);
void xe_render(xe_canvas *canvas);

/* This function should be called on each frame before writing data to any persistent coherent buffer. */
void xe_gfx_sync(void);
void xe_shader_reload(void);
void xe_spine_update(xe_spine *self, float delta_sec);
void xe_spine_draw(xe_spine *self);

#endif /* __XE_H__ */
