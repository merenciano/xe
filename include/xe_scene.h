#ifndef XE_SCENE_H
#define XE_SCENE_H

#include "xe_asset.h"
#include <llulu/lu_math.h>

/* API Scene graph */
typedef struct xe_scene_node {
    xe_handle hnd;
} xe_scene_node;

typedef struct xe_scene_node_desc {
    float pos_x;
    float pos_y;
    float pos_z;
    float scale;
} xe_scene_node_desc;

xe_scene_node xe_scene_create_node(xe_scene_node_desc *desc);
xe_scene_node xe_scene_create_drawable(xe_scene_node_desc *desc, xe_image img);
void xe_scene_drawable_draw_pass(void);
void xe_scene_update_world(void);
void xe_scene_register_node_update(xe_scene_node node, void *user_data, void (*update_fn)(xe_scene_node, void *));

/* API Transform */
float *xe_transform_init(xe_scene_node node, float px, float py, float pz, float scale);

const float *xe_transform_get(xe_scene_node node);
const float *xe_transform_get_global(xe_scene_node node);
void xe_transform_set(xe_scene_node node, const float *mat);
/* Transformations, *not* assignments */
const float *xe_transform_scale(xe_scene_node node, float k); /* uniform scalation in all axis */
const float *xe_transform_scale_v(xe_scene_node node, float x_factor, float y_factor, float z_factor);
const float *xe_transform_translate(xe_scene_node node, float x, float y, float z);
const float *xe_transform_rotate(xe_scene_node node, lu_vec3 axis, float rad);
/* Transform setters (overwrite) */
const float *xe_transform_set_pos(xe_scene_node node, float x, float y, float z);
const float *xe_transform_set_scale(xe_scene_node node, float x, float y, float z);
const float *xe_transform_set_rotation_x(xe_scene_node node, float rad);
const float *xe_transform_set_rotation_y(xe_scene_node node, float rad);
const float *xe_transform_set_rotation_z(xe_scene_node node, float rad);

#endif /* XE_SCENE_H */
