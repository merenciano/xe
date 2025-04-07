#include "xe_scene.h"
#include "xe_resource.h"
#include "xe_platform.h"

#include <llulu/lu_math.h>
#include <spine/extension.h>
#include <spine/spine.h>

enum {
    XE_SCENE_CAP = 64,
};

struct xe_graph_node {
    xe_resource res;
    int child_count;
};

static struct xe_graph_node g_nodes[XE_SCENE_CAP];
static lu_mat4 g_transforms[XE_SCENE_CAP];
//static const xe_mat4* g_scene_transforms = (const xe_mat4*)&tr_buf[0];
static int g_node_count;

float* xe_scene_tr_init(xe_scene_node node, float px, float py, float pz, float scale)
{
    float *tr = g_transforms[node.hnd].m;
    lu_mat4_identity(tr);
    lu_mat4_scale(tr, tr, (float[3]) { scale, scale, scale });
    return lu_mat4_translate(tr, tr, (float[3]) { px, py, pz });
}

#define XE_SCENE_ITER_STACK_CAP 64

/*
const xe_image_desc g_images[] = {
    { .id = {"owl"}, .img_path = "./assets/owl.png"},
    { .id = {"default"}, .img_path = "./assets/default_tex.png"},
    { .id = {"test"}, .img_path = "./assets/tex_test.png"},
};

const xe_shader_desc g_shaders[] = {
    { .id = {"generic"}, .vert_path = "./assets/vert.glsl", .frag_path =
"./assets/frag.glsl" },
};

const xe_spine_desc g_spines[] = {
    { .id = {"owl"}, .atlas_path = "./assets/owl.atlas", .skeleton_path =
"./assets/owl-pro.json" },
};

const xe_scenenode_desc g_nodes[] = {
    {
        .pos_x = 0.0f, .pos_y = 0.0f, .scale_x = 1.0f, .scale_y = 1.0f,
.rotation = 0.0f, .type = XE_NODE_SKELETON, .asset_id = {"owl"}, .child_count =
0
    }
};

enum {
    XE_ASSETS_IMG_COUNT = sizeof(g_images) / sizeof(g_images[0]),
    XE_ASSETS_SHADER_COUNT = sizeof(g_shaders) / sizeof(g_shaders[0]),
    XE_ASSETS_SPINE_COUNT = sizeof(g_spines) / sizeof(g_spines[0]),
    XE_SCENE_NODE_COUNT = sizeof(g_nodes) / sizeof(g_nodes[0])
};
*/

typedef struct xe_scene_iter_state_t {
    int remaining_children;
    int parent_index;
    lu_mat4 trw;
} xe_scene_iter_state_t;

typedef struct xe_scene_iter_stack_t {
    xe_scene_iter_state_t buf[XE_SCENE_ITER_STACK_CAP];
    size_t count;
} xe_scene_iter_stack_t;

static inline const xe_scene_iter_state_t*
xe_scene_get_state(xe_scene_iter_stack_t* stack)
{
    static const xe_scene_iter_state_t EMPTY = {
        .remaining_children = 0,
        .parent_index = -1,
        .trw = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
    };

    if (stack && stack->count > 0) {
        return &stack->buf[stack->count - 1];
    }
    return &EMPTY;
}

static inline bool xe_scene_pop_state(xe_scene_iter_stack_t* stack)
{
    xe_assert(stack);
    xe_assert(stack->count > 0);
    return stack->count--;
}

static inline void xe_scene_push_state(xe_scene_iter_stack_t* stack,
    int node_idx)
{
    const xe_scene_iter_state_t* curr = xe_scene_get_state(stack);
    const struct xe_graph_node* node = &g_nodes[node_idx];
    stack->buf[stack->count].remaining_children = node->child_count;
    stack->buf[stack->count].parent_index = node_idx;
    lu_mat4_multiply(stack->buf[stack->count].trw.m, curr->trw.m,
        g_transforms[node_idx].m);
    stack->count++;
    assert(stack->count < XE_SCENE_ITER_STACK_CAP);
}

static inline bool xe_scene_step_state(xe_scene_iter_stack_t* stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

xe_scene_node xe_scene_node_create(xe_resource *out_res)
{
    int i = g_node_count++;
    lu_mat4_identity(g_transforms[i].m);
    g_nodes[i].child_count = 0;
    if (out_res) {
        out_res = &g_nodes[i].res;
    }

    return (xe_scene_node){ .hnd = i };
}

void xe_scene_draw(void)
{
    xe_scene_iter_stack_t state = { .count = 0 };
    for (int i = 0; i < g_node_count; ++i) {
        if (g_nodes[i].child_count > 0) {
            xe_scene_push_state(&state, i);
        } else {
            if (!xe_scene_step_state(&state)) {
                if (!xe_scene_pop_state(&state)) {
                    assert((i + 1) == XE_SCENE_CAP && "The scene graph only has one root node, so the current has to "
                                                    "be the last one of the vector.");
                    break;
                }
            }
        }

        if (g_nodes[i].res.mask & XE_RP_DRAWABLE) {
            const xe_scene_iter_state_t* curr = xe_scene_get_state(&state);
            xe_mat4 model;
            lu_mat4_multiply(model.m, curr->trw.m, g_transforms[i].m);
            g_nodes[i].res.vt->draw(&g_nodes[i].res, &model);
        }
    }
}
