#ifndef __ORXATA_H__
#define __ORXATA_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
    TODO:
     -> Node hierarchy (transform matrix)
     -> Spine runtime
     -> Text rendering
*/

typedef struct orx_config_t {
    void *(*gl_loader)(const char *);
    const char *title;
    int canvas_width;
    int canvas_height;
    const char *vert_shader_path;
    const char *frag_shader_path;
    float seconds_between_shader_file_changed_checks;
} orx_config_t;

typedef uint16_t orx_index_t;

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

typedef struct orx_shape_t {
    int base_vtx;
    int first_idx;
    int idx_count;
    int draw_index;
    orx_texture_t texture;
} orx_shape_t;

typedef struct orx_node_t {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    orx_shape_t shape;

#ifdef ORX_DEBUG
    const char *name;
#endif
} orx_node_t;

typedef struct orx_spine_t {
    orx_node_t node;
    struct spSkeleton *skel;
    struct spAnimationState *anim;
} orx_spine_t;


void orx_init(orx_config_t *config);
orx_image_t orx_load_image(const char *path);
orx_shape_t orx_mesh_add(const orx_vertex_t *vtx_data, int vtx_count, const orx_index_t *idx_data, int idx_count);
orx_texture_t orx_texture_reserve(orx_texture_format_t format);
void orx_texture_set(orx_texture_t tex, void *data);
void orx_draw_node(orx_node_t *node);
void orx_render(void);

/* Extended API */
void orx_shader_reload(void);
void orx_spine_update(orx_spine_t *self, float delta_sec);
void orx_spine_draw(orx_spine_t *self);

#endif /* __ORXATA_H__ */
