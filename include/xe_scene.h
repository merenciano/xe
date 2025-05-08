
#ifndef __XE_SCENE_H__
#define __XE_SCENE_H__

#include "xe_renderer.h"

/* API Handle */
typedef unsigned int xe_handle;

/* API Image */
typedef struct {xe_handle id;} xe_image;
enum {
    /* Compile-time config constants */
    XE_MAX_IMAGES = 32,

    /* Flags */
    XE_IMG_PREMUL_ALPHA = 0x0001,
};

xe_image xe_image_load(const char *path, int tex_flags); // XE_IMG_ ... 

/* API Scene graph */
typedef struct xe_scene_node {
    xe_handle hnd;
} xe_scene_node;

typedef struct xe_scene_node_desc {
    float pos_x;
    float pos_y;
    float pos_z;
    float scale;

    int (*draw_fn)(lu_mat4 *tr, void *ctx);
    void *draw_ctx;
} xe_scene_node_desc;

xe_scene_node xe_scene_create_node(xe_scene_node_desc *desc);


/* API Transform component */
float *xe_scene_tr_init(xe_scene_node node, float px, float py, float pz, float scale);

/* Returns a reference *NOT SUITABLE STORING OR EXPOSING*.
 * Copy the matrix contents for anything other than reading as rvalue, since
 * not only the value but its memory location is likely to change (from other threads).
 */
const float *xe_transform_get(xe_scene_node node);
void xe_transform_set(xe_scene_node node, const float *mat);
/* Transformations, *not* assignments */
const float *xe_transform_scale(xe_scene_node node, float k); /* uniform scalation in all axis */
const float *xe_transform_scale_v(xe_scene_node node, float x_factor, float y_factor, float z_factor);
const float *xe_transform_translate(xe_scene_node node, float x, float y, float z);
/* Transform setters (overwrite) */
const float *xe_transform_set_pos(xe_scene_node node, float x, float y, float z);
const float *xe_transform_set_scale(xe_scene_node node, float x, float y, float z);
const float *xe_transform_set_rotation_x(xe_scene_node node, float rad);
const float *xe_transform_set_rotation_y(xe_scene_node node, float rad);
const float *xe_transform_set_rotation_z(xe_scene_node node, float rad);

/* API Shape component */
xe_scene_node xe_scene_create_drawable(xe_scene_node_desc *desc, xe_image img, xe_rend_mesh mesh);
void xe_scene_draw(void);

// API MOCK
enum {
    XE_CFG_MAX_ENTITIES = 256,
    XE_CFG_MAX_RESOURCES = 64,
    XE_CFG_MAX_COMPONENT_TYPES = 64,
    XE_CFG_MAX_SCENE_GRAPH_DEPTH = 64,
};

enum { /* Component identifiers */
    XE_COMP_SGNODE, /* scene graph node: transform, hierarchy, visibility */
    XE_COMP_SHAPE, /* drawable shape: mesh, material */
    XE_COMP_CALLBACK, /* callbacks for trace events (init, update, print) */
    XE_COMP_SCRIPT, /*  */
    XE_COMP_BUILTIN_COUNT,
    XE_COMP_MAX = 64
};

typedef struct {xe_handle hnd;} xe_entity;
xe_entity xe_ent_create(const char *name);
void xe_ent_delete(xe_entity entity);
void xe_ent_add_comp(xe_entity entity, int type);
void xe_ent_rm_comp(xe_entity entity, int type);

#endif /* __XE_SCENE_H__ */
