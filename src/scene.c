#if 0
#include "scene.h"

#define ORX_SCENE_ITER_STACK_CAP 64

const orx_image_desc g_images[] = {
    { .id = {"owl"}, .img_path = "./assets/owl.png"},
    { .id = {"default"}, .img_path = "./assets/default_tex.png"},
    { .id = {"test"}, .img_path = "./assets/tex_test.png"},
};

const orx_shader_desc g_shaders[] = {
    { .id = {"generic"}, .vert_path = "./assets/vert.glsl", .frag_path = "./assets/frag.glsl" },
};

const orx_spine_desc g_spines[] = {
    { .id = {"owl"}, .atlas_path = "./assets/owl.atlas", .skeleton_path = "./assets/owl-pro.json" },
};

const orx_scenenode_desc g_nodes[] = {
    { 
        .pos_x = 0.0f, .pos_y = 0.0f, .scale_x = 1.0f, .scale_y = 1.0f, .rotation = 0.0f,
        .type = ORX_NODE_SKELETON, .asset_id = {"owl"}, .child_count = 0
    }
};

enum {
    ORX_ASSETS_IMG_COUNT = sizeof(g_images) / sizeof(g_images[0]),
    ORX_ASSETS_SHADER_COUNT = sizeof(g_shaders) / sizeof(g_shaders[0]),
    ORX_ASSETS_SPINE_COUNT = sizeof(g_spines) / sizeof(g_spines[0]),
    ORX_SCENE_NODE_COUNT = sizeof(g_nodes) / sizeof(g_nodes[0])
};

typedef struct orx_scene_iter_state_t {
    int remaining_children;
    int parent_index;
    float abpx; /* absolute scale, position and rotation values */
    float abpy;
    float absx;
    float absy;
    float abrot;
} orx_scene_iter_state_t;

typedef struct orx_scene_iter_stack_t {
    orx_scene_iter_state_t buf[ORX_SCENE_ITER_STACK_CAP];
    size_t count;
} orx_scene_iter_stack_t;

static inline const orx_scene_iter_state_t *
orx_scene_get_state(orx_scene_iter_stack_t *stack)
{
    static const orx_scene_iter_state_t EMPTY = { .remaining_children = 0, .parent_index = -1, .abpx = 0.0f, .abpy = 0.0f, .absx = 1.0f, .absy = 1.0f, .abrot = 0.0f };
    if (stack && stack->count > 0) {
        return &stack->buf[stack->count - 1];
    }
    return &EMPTY;
}

static inline bool
orx_scene_pop_state(orx_scene_iter_stack_t *stack)
{
    assert(stack);
    assert(stack->count > 0);
    return stack->count--;
}

static inline void
orx_scene_push_state(orx_scene_iter_stack_t *stack, int node_idx)
{
    const orx_scene_iter_state_t *curr = orx_scene_get_state(stack);
    const orx_scenenode_desc *node = &g_nodes[node_idx];
    stack->buf[stack->count].remaining_children = node->child_count;
    stack->buf[stack->count].parent_index = node_idx;
    stack->buf[stack->count].abrot = curr->abrot + node->rotation;
    stack->buf[stack->count].abpx = curr->abpx + node->pos_x;
    stack->buf[stack->count].abpy = curr->abpy + node->pos_y;
    stack->buf[stack->count].absx = curr->absx * node->scale_x;
    stack->buf[stack->count].absy = curr->absy * node->scale_y;
    stack->count++;
    assert(stack->count < ORX_SCENE_ITER_STACK_CAP);
}

static inline bool
orx_scene_step_state(orx_scene_iter_stack_t *stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

static inline void
orx_scene_update(void)
{
    orx_scene_iter_stack_t state = { .count = 0 };
    for (int i = 0; i < ORX_SCENE_NODE_COUNT; ++i) {
        if (g_nodes[i].child_count > 0) {
            orx_scene_push_state(&state, i);

            if (g_nodes[i].type & (ORX_NODE_SHAPE | ORX_NODE_SKELETON)) {
                const orx_scene_iter_state_t *curr = orx_scene_get_state(&state);
                /*
                orx_gfx_process((orx_drawable_t){
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
        if (!orx_scene_step_state(&state)) {
            if (!orx_scene_pop_state(&state)) {
                assert((i + 1) == ORX_SCENE_NODE_COUNT && "The scene graph only has one root node, so the current has to be the last one of the vector."); 
                break;
            }
        }
    }
}

#endif