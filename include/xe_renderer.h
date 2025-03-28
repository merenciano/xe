#ifndef __XE_RENDERER_H__
#define __XE_RENDERER_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct xe_rend_canvas {
    int w;
    int h;
} xe_rend_canvas;

typedef struct xe_rend_config {
    void *(*gl_loader)(const char *);
    xe_rend_canvas canvas;
    float seconds_between_shader_file_changed_checks;
    const char *vert_shader_path;
    const char *frag_shader_path;
} xe_rend_config;

typedef uint16_t xe_rend_idx;
typedef int xe_rend_draw_id;

enum xe_rend_tex_flags {
    XE_TEX_DEPTH = 0x01,
    XE_TEX_STENCIL = 0x02,
    XE_TEX_NEAREST = 0x04, // filter linear if unset
    XE_TEX_REPEAT = 0x8, // clamp to edge if unset
    XE_TEX_MIRROR = 0x10, // if repeat: mirror
    XE_TEX_BORDER = 0x20, // if not repeat: clamp to border
};

enum xe_rend_tex_pixel_format {
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

typedef struct xe_rend_texfmt {
    uint16_t width;
    uint16_t height;
    uint16_t format; /* see: enum xe_rend_tex_pixel_format */
    uint16_t flags; /* see: enum xe_rend_tex_flags */
} xe_rend_texfmt;

typedef struct xe_rend_tex {
    int idx;
    int layer;
} xe_rend_tex;

typedef struct xe_rend_img {
    const char *path;
    void *data;
    int w;
    int h;
    int channels;
    xe_rend_tex tex;
} xe_rend_img;

typedef struct xe_rend_vtx {
    float x;
    float y;
    float u;
    float v;
    uint32_t color;
} xe_rend_vtx;

typedef struct xe_rend_mesh {
    int base_vtx;
    int first_idx;
    int idx_count;
} xe_rend_mesh;

typedef struct xe_rend_material {
    /* absolute transform values */
    float apx;
    float apy;
    float asx;
    float asy;
    float arot;
    xe_rend_tex tex;
} xe_rend_material;

void xe_rend_canvas_resize(int new_w, int new_h);
bool xe_rend_init(xe_rend_config *config);
xe_rend_img xe_rend_img_load(const char *path);
xe_rend_tex xe_rend_tex_reserve(xe_rend_texfmt format);
void xe_rend_tex_set(xe_rend_tex tex, void *data);
xe_rend_mesh xe_rend_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size);
xe_rend_draw_id xe_rend_material_add(xe_rend_material mat);
void xe_rend_submit(xe_rend_mesh mesh, xe_rend_draw_id drawidx);
void xe_rend_render();

/* This function should be called on each frame before writing data to any persistent coherent buffer. */
void xe_rend_sync(void);
void xe_rend_shader_reload(void);
void xe_rend_shutdown(void);

#endif /* __XE_RENDERER_H__ */
