#ifndef __XE_GFX_H__
#define __XE_GFX_H__

#include <llulu/lu_math.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct xe_gfx_canvas {
    int w;
    int h;
} xe_gfx_canvas;

typedef struct xe_gfx_config {
    void *(*gl_loader)(const char *);
    xe_gfx_canvas canvas;
    const char *vert_shader_path;
    const char *frag_shader_path;
} xe_gfx_config;

typedef uint16_t xe_gfx_idx;

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

typedef struct xe_gfx_vtx {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} xe_gfx_vtx;

typedef struct xe_gfx_material {
    lu_mat4 model;
    lu_vec4 color; // TODO: Remove since it's in the vertex already
    lu_vec4 darkcolor;
    xe_gfx_tex tex;
    float pma;
} xe_gfx_material;

bool xe_gfx_init(xe_gfx_config *config);

/* This function should be called every frame before writing data to any persistent coherent buffer. */
void xe_gfx_sync(void);

/* Flushes, unmaps and deletes gpu resources. It is up to the programmer to call or skip this function. */
void xe_gfx_shutdown(void);

xe_gfx_tex xe_gfx_tex_alloc(xe_gfx_texfmt format);
void xe_gfx_tex_load(xe_gfx_tex tex, void *data);

/* Adds  */
void xe_gfx_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, xe_gfx_material *material);
void xe_gfx_render();


#endif /* __XE_GFX_H__ */
