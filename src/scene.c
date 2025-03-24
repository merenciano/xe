#if 0
#include "scene.h"

#define XE_SCENE_ITER_STACK_CAP 64

const xe_image_desc g_images[] = {
    { .id = {"owl"}, .img_path = "./assets/owl.png"},
    { .id = {"default"}, .img_path = "./assets/default_tex.png"},
    { .id = {"test"}, .img_path = "./assets/tex_test.png"},
};

const xe_shader_desc g_shaders[] = {
    { .id = {"generic"}, .vert_path = "./assets/vert.glsl", .frag_path = "./assets/frag.glsl" },
};

const xe_spine_desc g_spines[] = {
    { .id = {"owl"}, .atlas_path = "./assets/owl.atlas", .skeleton_path = "./assets/owl-pro.json" },
};

const xe_scenenode_desc g_nodes[] = {
    { 
        .pos_x = 0.0f, .pos_y = 0.0f, .scale_x = 1.0f, .scale_y = 1.0f, .rotation = 0.0f,
        .type = XE_NODE_SKELETON, .asset_id = {"owl"}, .child_count = 0
    }
};

enum {
    XE_ASSETS_IMG_COUNT = sizeof(g_images) / sizeof(g_images[0]),
    XE_ASSETS_SHADER_COUNT = sizeof(g_shaders) / sizeof(g_shaders[0]),
    XE_ASSETS_SPINE_COUNT = sizeof(g_spines) / sizeof(g_spines[0]),
    XE_SCENE_NODE_COUNT = sizeof(g_nodes) / sizeof(g_nodes[0])
};

typedef struct xe_scene_iter_state_t {
    int remaining_children;
    int parent_index;
    float abpx; /* absolute scale, position and rotation values */
    float abpy;
    float absx;
    float absy;
    float abrot;
} xe_scene_iter_state_t;

typedef struct xe_scene_iter_stack_t {
    xe_scene_iter_state_t buf[XE_SCENE_ITER_STACK_CAP];
    size_t count;
} xe_scene_iter_stack_t;

static inline const xe_scene_iter_state_t *
xe_scene_get_state(xe_scene_iter_stack_t *stack)
{
    static const xe_scene_iter_state_t EMPTY = { .remaining_children = 0, .parent_index = -1, .abpx = 0.0f, .abpy = 0.0f, .absx = 1.0f, .absy = 1.0f, .abrot = 0.0f };
    if (stack && stack->count > 0) {
        return &stack->buf[stack->count - 1];
    }
    return &EMPTY;
}

static inline bool
xe_scene_pop_state(xe_scene_iter_stack_t *stack)
{
    assert(stack);
    assert(stack->count > 0);
    return stack->count--;
}

static inline void
xe_scene_push_state(xe_scene_iter_stack_t *stack, int node_idx)
{
    const xe_scene_iter_state_t *curr = xe_scene_get_state(stack);
    const xe_scenenode_desc *node = &g_nodes[node_idx];
    stack->buf[stack->count].remaining_children = node->child_count;
    stack->buf[stack->count].parent_index = node_idx;
    stack->buf[stack->count].abrot = curr->abrot + node->rotation;
    stack->buf[stack->count].abpx = curr->abpx + node->pos_x;
    stack->buf[stack->count].abpy = curr->abpy + node->pos_y;
    stack->buf[stack->count].absx = curr->absx * node->scale_x;
    stack->buf[stack->count].absy = curr->absy * node->scale_y;
    stack->count++;
    assert(stack->count < XE_SCENE_ITER_STACK_CAP);
}

static inline bool
xe_scene_step_state(xe_scene_iter_stack_t *stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

static inline void
xe_scene_update(void)
{
    xe_scene_iter_stack_t state = { .count = 0 };
    for (int i = 0; i < XE_SCENE_NODE_COUNT; ++i) {
        if (g_nodes[i].child_count > 0) {
            xe_scene_push_state(&state, i);

            if (g_nodes[i].type & (XE_NODE_SHAPE | XE_NODE_SKELETON)) {
                const xe_scene_iter_state_t *curr = xe_scene_get_state(&state);
                /*
                xe_gfx_process((xe_drawable_t){
                    .apx = curr->abpx,
                    .apy = curr->abpy,
                    .asx = curr->absx,
                    .asy = curr->absy,
                    .arot = curr->abrot,
                    .id = i
                });
                */
            }
            continue;
        }
        // TODO populate vertex buffer, uniform buffer and multidraw command buffer with the node if it has shape.

        // UPDATE NODE
        if (!xe_scene_step_state(&state)) {
            if (!xe_scene_pop_state(&state)) {
                assert((i + 1) == XE_SCENE_NODE_COUNT && "The scene graph only has one root node, so the current has to be the last one of the vector."); 
                break;
            }
        }
    }
}

#endif