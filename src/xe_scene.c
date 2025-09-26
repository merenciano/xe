#include "xe_scene.h"
#include "xe_scene_internal.h"
#include "xe_gfx.h"

#include <llulu/lu_defs.h>
#include <llulu/lu_math.h>
#include <llulu/lu_error.h>
#include <llulu/lu_log.h>

#include <string.h>

enum {
    XE_SCENE_CAP = 64,
    XE_CFG_MAX_SCENE_GRAPH_DEPTH = 64
};

struct xe_graph_node {
    struct xe_resource res;
    int transform_index;
    int child_count;
};

struct xe_graph_drawable {
    xe_scene_node node;
    xe_image img;
};

struct xe_scene_node_update {
    xe_scene_node node;
    void *user_data;
    void (*update_fn)(xe_scene_node, void *);
};

static const xe_gfx_vtx QUAD_VERTICES[] = {
    { .x = -1.0f, .y = -1.0f,
      .u = 0.0f, .v = 0.0f,
      .color = 0xFFFFFFFF },
    { .x = -1.0f, .y = 1.0f,
      .u = 0.0f, .v = 1.0f,
      .color = 0xFFFFFFFF },
    { .x = 1.0f, .y = 1.0f,
      .u = 1.0f, .v = 1.0f,
      .color = 0xFFFFFFFF },
    { .x = 1.0f, .y = -1.0f,
      .u = 1.0f, .v = 0.0f,
      .color = 0xFFFFFFFF }
};

static const xe_gfx_idx QUAD_INDICES[] = { 0, 1, 2, 0, 2, 3 };

static struct xe_graph_node g_nodes[XE_SCENE_CAP];
static lu_mat4 g_transforms[XE_SCENE_CAP];
static lu_mat4 global_transforms[XE_SCENE_CAP];
static int g_node_count;
static struct xe_graph_drawable g_drawables[XE_SCENE_CAP];
static int g_drawable_count;
static struct xe_scene_node_update g_updates[XE_SCENE_CAP];
static int g_update_count;

void
xe_scene_register_node_update(xe_scene_node node, void *user_data, void (*update_fn)(xe_scene_node, void *))
{
    g_updates[g_update_count].node = node;
    g_updates[g_update_count].update_fn = update_fn;
    g_updates[g_update_count].user_data = user_data;
    g_update_count++;
}

void
xe_scene_dispatch_updates(void)
{
    for (int i = 0; i < g_update_count; i++) {
        g_updates[i].update_fn(g_updates[i].node, g_updates[i].user_data);
    }
}

static const struct xe_graph_node *
get_node(xe_scene_node n)
{
    lu_err_assert(xe_res_index(n.hnd) < g_node_count && "Node index out of range.");
    return g_nodes + xe_res_index(n.hnd);
}

static lu_mat4 *
get_tr(xe_scene_node n)
{
    lu_err_assert(get_node(n)->transform_index < g_node_count && "Transform index out of range.");
    return g_transforms + get_node(n)->transform_index;
}

static lu_mat4 *
get_global_tr(xe_scene_node n)
{
    lu_err_assert(get_node(n)->transform_index < g_node_count && "Transform index out of range.");
    return global_transforms + get_node(n)->transform_index;
}

xe_scene_node xe_scene_create_node(xe_scene_node_desc *desc)
{
    int index = g_node_count++;
    g_nodes[index].child_count = 0;
    g_nodes[index].transform_index = index;
    g_nodes[index].res.state = 0;
    g_nodes[index].res.version++;
    xe_scene_node node = { .hnd = xe_res_handle_gen(g_nodes[index].res.version, index)};
    xe_scene_tr_init(node, desc->pos_x, desc->pos_y, desc->pos_z, desc->scale);
    return node;
}

int
xe_drawable_draw(lu_mat4 *tr, void *draw_ctx)
{
    if (!draw_ctx) {
        lu_log_err("draw_ctx = NULL, ignoring xe_drawable_draw call.");
        return LU_ERR_BADARG;
    }
    struct xe_graph_drawable *node = draw_ctx;
    xe_gfx_material material = (xe_gfx_material){.model = *tr, .color = LU_VEC(1.0f, 1.0f, 1.0f, 1.0f), .darkcolor = LU_VEC(0.0f, 0.0f, 0.0f, 1.0f), .tex = xe_image_ptr(node->img)->tex, .pma = 0};
    xe_gfx_push(QUAD_VERTICES, sizeof(QUAD_VERTICES), QUAD_INDICES, sizeof(QUAD_INDICES), &material);
    return LU_ERR_SUCCESS;
}

void
xe_scene_drawable_draw_pass(void)
{
    int count = g_drawable_count;
    for (int i = 0; i < count; ++i) {
        xe_drawable_draw((lu_mat4*)xe_transform_get_global(g_drawables[i].node), &g_drawables[i]);

    }
}

xe_scene_node
xe_scene_create_drawable(xe_scene_node_desc *desc, xe_image img)
{
    struct xe_graph_drawable *node = &g_drawables[g_drawable_count++];
    node->img = img;
    node->node = xe_scene_create_node(desc);
    return node->node;
}

/* Scene graph iteration state */

typedef struct xe_scene_iter_state_t {
    int remaining_children;
    const float *parent_global_transform;
    lu_mat4 trw;
} xe_scene_iter_state_t;

typedef struct xe_scene_iter_stack_t {
    xe_scene_iter_state_t buf[XE_CFG_MAX_SCENE_GRAPH_DEPTH];
    size_t count;
} xe_scene_iter_stack_t;

static const xe_scene_iter_state_t*
xe_scene_get_state(xe_scene_iter_stack_t* stack)
{
    if (stack && stack->count > 0) {
        return &stack->buf[stack->count - 1];
    }
    return NULL;
}

static bool
xe_scene_pop_state(xe_scene_iter_stack_t* stack)
{
    lu_err_assert(stack);
    if (--stack->count < 1) {
        stack->count = 1;
    }

    return stack->count > 1;
}

static void
xe_scene_push_state(xe_scene_iter_stack_t* stack, int node_idx, float *global_tr)
{
    const struct xe_graph_node* node = &g_nodes[node_idx];
    stack->buf[stack->count].remaining_children = node->child_count;
    stack->buf[stack->count].parent_global_transform = global_tr;
    stack->count++;
    lu_err_ensures(stack->count < XE_CFG_MAX_SCENE_GRAPH_DEPTH);
}

static bool
xe_scene_step_state(xe_scene_iter_stack_t* stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

void
xe_scene_update_world(void)
{
    static const lu_mat4 IDENTITY_MAT4 = {.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f }};

    xe_scene_iter_stack_t state = {
        .buf = {{
            .remaining_children = g_node_count,
            .parent_global_transform = IDENTITY_MAT4.m
        }},
        .count = 1
    };

    xe_scene_dispatch_updates();

    for (int i = 0; i < g_node_count; ++i) {
        struct xe_graph_node *node = g_nodes + i;
        const xe_scene_iter_state_t* curr = xe_scene_get_state(&state);
        float *global_tr = global_transforms[node->transform_index].m;
        float *local_tr = g_transforms[node->transform_index].m;
        lu_mat4_multiply(global_tr, curr->parent_global_transform, local_tr);

        if (node->child_count > 0) {
            xe_scene_push_state(&state, i, global_tr);
        } else {
            if (!xe_scene_step_state(&state)) {
                if (!xe_scene_pop_state(&state)) {
                    lu_err_assert((i + 1) == g_node_count && "The scene graph only has one root node, so the current has to be the last one of the vector.");
                    break;
                }
            }
        }
    }
}

float* xe_scene_tr_init(xe_scene_node node, float px, float py, float pz, float scale)
{
    float *tr = g_transforms[g_nodes[xe_res_index(node.hnd)].transform_index].m;
    lu_mat4_identity(tr);
    lu_mat4_scale(tr, tr, (float[3]) { scale, scale, scale });
    return lu_mat4_translate(tr, tr, (float[3]) { px, py, pz });
}

const float *
xe_transform_get(xe_scene_node node)
{
    return get_tr(node)->m;
}

const float *
xe_transform_get_global(xe_scene_node node)
{
    return get_global_tr(node)->m;
}

void
xe_transform_set(xe_scene_node node, const float *mat)
{
    memcpy(get_tr(node)->m, mat, sizeof(lu_mat4));
}

const float *
xe_transform_translate(xe_scene_node node, float x, float y, float z)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_translate(tr, tr, (float[3]){x, y, z});
}

const float *
xe_transform_scale(xe_scene_node node, float k)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_scale(tr, tr, &LU_VEC(k, k, k).x);
}

const float *
xe_transform_scale_v(xe_scene_node node, float x_factor, float y_factor, float z_factor)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_scale(tr, tr, &LU_VEC(x_factor, y_factor, z_factor).x);

}

const float *
xe_transform_set_rotation_x(xe_scene_node node, float rad)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_rotation_x(tr, rad);
}

const float *
xe_transform_set_rotation_y(xe_scene_node node, float rad)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_rotation_y(tr, rad);
}

const float *
xe_transform_set_rotation_z(xe_scene_node node, float rad)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_rotation_z(tr, rad);
}

const float *
xe_transform_set_pos(xe_scene_node node, float x, float y, float z)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_translation(tr, tr, &LU_VEC(x, y, z).x);
}

const float *
xe_transform_set_scale(xe_scene_node node, float x, float y, float z)
{
    float *tr = get_tr(node)->m;
    return lu_mat4_scaling(tr, tr, &LU_VEC(x, y, z).x);
}

const float *
xe_transform_rotate(xe_scene_node node, lu_vec3 axis, float rad)
{
    float *tr = get_tr(node)->m;
    float mat[16];
    return lu_mat4_multiply(tr, tr, lu_mat4_rotation_axis(mat, &axis.x, rad));
}

