#ifndef __ORXATA_H__
#define __ORXATA_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
    TODO:
     -> Renaming
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

typedef struct orx_canvas_t {
    int w;
    int h;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    float bg_col[3];
} orx_canvas_t;

typedef struct orx_config_t {
    void *(*gl_loader)(const char *);
    orx_canvas_t canvas;
    float seconds_between_shader_file_changed_checks;
    const char *vert_shader_path;
    const char *frag_shader_path;
} orx_config_t;

typedef uint16_t orx_index_t;
typedef int orx_draw_idx;

enum { /* Texture flags */
    ORX_TEX_DEPTH = 0x01,
    ORX_TEX_STENCIL = 0x02,
    ORX_TEX_NEAREST = 0x04, // filter linear if unset
    ORX_TEX_REPEAT = 0x8, // clamp to edge if unset
    ORX_TEX_MIRROR = 0x10, // if repeat: mirror
    ORX_TEX_BORDER = 0x20, // if not repeat: clamp to border
};

enum { /* Pixel format */
    ORX_PIXFMT_R = 0,
    ORX_PIXFMT_RG,
    ORX_PIXFMT_RGB,
    ORX_PIXFMT_SRGB,
    ORX_PIXFMT_RGBA,
    ORX_PIXFMT_R_F16,
    ORX_PIXFMT_RG_F16,
    ORX_PIXFMT_RGB_F16,
    ORX_PIXFMT_RGBA_F16,
    ORX_PIXFMT_R_F32,
    ORX_PIXFMT_RG_F32,
    ORX_PIXFMT_RGB_F32,
    ORX_PIXFMT_RGBA_F32,
    ORX_PIXFMT_COUNT
};

typedef struct orx_texture_format_t {
    uint16_t width;
    uint16_t height;
    uint16_t pixel_fmt; /* see: ORX_PIXFMT_ */
    uint16_t flags; /* see: ORX_TEX_ */
} orx_texture_format_t;

typedef struct orx_texture_t {
    int idx;
    int layer;
} orx_texture_t;

typedef struct orx_image_t {
    const char *path;
    void *data;
    int w;
    int h;
    int channels;
    orx_texture_t tex;
} orx_image_t;

typedef struct orx_vertex_t {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} orx_vertex_t;

typedef struct orx_mesh_t {
    int base_vtx;
    int first_idx;
    int idx_count;
} orx_mesh_t;

typedef struct orx_node_t {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation;
    orx_mesh_t mesh; /* TODO: Change to enum: SHAPE_QUAD, SHAPE_SPINE... */
    orx_texture_t tex;

#ifdef ORX_DEBUG
    const char *name;
#endif
} orx_node_t;

typedef struct orx_material_t {
    /* absolute transform values */
    float apx;
    float apy;
    float asx;
    float asy;
    float arot;
    orx_texture_t tex;
} orx_material_t;

typedef struct orx_spine_t {
    orx_node_t node;
    struct spSkeleton *skel;
    struct spAnimationState *anim;
} orx_spine_t;

bool orx_init(orx_config_t *config);
orx_image_t orx_load_image(const char *path);
orx_texture_t orx_texture_reserve(orx_texture_format_t format);
void orx_texture_set(orx_texture_t tex, void *data);
orx_mesh_t orx_gfx_add_mesh(const void *vert, size_t vert_size, const void *indices, size_t indices_size);
orx_draw_idx orx_gfx_add_material(orx_material_t mat);
void orx_gfx_submit(orx_mesh_t mesh, orx_draw_idx drawidx);
void orx_render(orx_canvas_t *canvas);

/* This function should be called on each frame before writing data to any persistent coherent buffer. */
void orx_gfx_sync(void);
void orx_shader_reload(void);
void orx_spine_update(orx_spine_t *self, float delta_sec);
void orx_spine_draw(orx_spine_t *self);


/*
    # Desired API
    Renderer:
        bool orx_init(orx_config);
        tex_t tex_add(tex_fmt_t);
        tex_t tex_set(tex_t, const void *data);

        mesh_t mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size);
        shape_t material_add(transform_t, material_t);
        bool drawcmd_add(mesh_t, shape_t);

        void 

*/

#endif /* __ORXATA_H__ */
