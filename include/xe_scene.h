
#ifndef __XE_SCENE_H__
#define __XE_SCENE_H__

#include "xe.h"
#include "xe_renderer.h"

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
xe_scene_node xe_scene_create_drawable(xe_scene_node_desc *desc, xe_image img, xe_rend_mesh mesh);
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

void xe_scene_draw(void);

#endif /* __XE_SCENE_H__ */