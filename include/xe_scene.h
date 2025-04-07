
#ifndef __XE_SCENE_H__
#define __XE_SCENE_H__

#include "xe.h"
#include "xe_renderer.h"

typedef struct xe_scene_node {
    int hnd;
} xe_scene_node;

float *xe_scene_tr_init(xe_scene_node node, float px, float py, float pz, float scale);

//extern const xe_mat4 *g_scene_transforms;
//void xe_scene_transform_pass(void);
void xe_scene_draw(void);

#endif /* __XE_SCENE_H__ */