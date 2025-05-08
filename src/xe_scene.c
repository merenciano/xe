#include "scene.h"
#include "xe_scene.h"
#include "xe_renderer.h"
#include "xe_platform.h"

#include <llulu/lu_defs.h>
#include <llulu/lu_math.h>
#include <llulu/lu_str.h>

#include <string.h>


/*
 * FUTURE DATA
 */

enum xe_ent_flags {
    XE_ENT_FREE = 0x0001,
};

struct xe_ent {
    uint64_t comp_mask;
    int ver;
    uint32_t flags;
};

struct xe_comp_pool {
    void *data; /* array of sequential components */
    size_t count;
    size_t elem_sz; /* in bytes */
    size_t capacity; /* in bytes */
    void (*init)(void *self);
};

struct xe_ent_names {
    lu_sstr names[XE_CFG_MAX_ENTITIES];
    uint16_t indices[XE_CFG_MAX_ENTITIES];
};

struct xe_res_names {
    lu_sstr names[XE_CFG_MAX_RESOURCES];
    uint16_t indices[XE_CFG_MAX_RESOURCES];
};

struct xe_scene_data {
    struct xe_ent_names names_ent;
    struct xe_ent_names names_res;
    struct xe_ent ents[XE_CFG_MAX_ENTITIES];
    struct xe_resource res[XE_CFG_MAX_RESOURCES];
    struct xe_comp_pool comp[XE_COMP_MAX];
    int last_ent;
    int comp_type_count;
    char *mem_head;
    char mem_arena[XE_CFG_SCENE_MEM_ARENA_BYTES];
};

void xe_scene_init(struct xe_scene_data *scene)
{
    memset(scene, 0, sizeof(*scene));
    scene->mem_head = scene->mem_arena;
    //scene->comp[XE_COMP_SGNODE].elem_sz = sizeof()
    scene->comp[XE_COMP_STATE].elem_sz = sizeof(xe_comp_state);
    scene->comp[XE_COMP_STATE].capacity = sizeof(xe_comp_state);
    scene->comp[XE_COMP_STATE].data = scene->mem_head;
    scene->mem_head += scene->comp[XE_COMP_STATE].capacity * scene->comp[XE_COMP_STATE].elem_sz;
    scene->comp_type_count = XE_COMP_BUILTIN_COUNT;
}

/*
 * END FUTURE DATA
 */

static const xe_rend_vtx QUAD_VERTICES[] = {
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

static const xe_rend_idx QUAD_INDICES[] = { 0, 1, 2, 0, 2, 3 };

enum {
    XE_SCENE_CAP = 64,
};

struct xe_graph_node {
    struct xe_resource res;
    int transform_index;
    int child_count;
};

struct xe_graph_drawable {
    xe_scene_node node;
    xe_image img;
    xe_rend_mesh mesh;
};

static struct xe_graph_node g_nodes[XE_SCENE_CAP];
static lu_mat4 g_transforms[XE_SCENE_CAP];
static int g_node_count;
static struct xe_graph_drawable g_drawables[XE_SCENE_CAP];
static int g_drawable_count;

static inline const struct xe_graph_node *
get_node(xe_scene_node n)
{
    xe_assert(xe_res_index(n.hnd) < g_node_count && "Node index out of range.");
    return g_nodes + xe_res_index(n.hnd);
}

static inline lu_mat4 *
get_tr(xe_scene_node n)
{
    xe_assert(get_node(n)->transform_index < g_node_count && "Transform index out of range.");
    return g_transforms + get_node(n)->transform_index;
}

xe_scene_node xe_scene_create_node(xe_scene_node_desc *desc)
{
    int index = g_node_count++;
    g_nodes[index].child_count = 0;
    g_nodes[index].transform_index = index;
    g_nodes[index].res.state = 0;
    g_nodes[index].res.version++;
    g_nodes[index].res.vt.draw = desc->draw_fn;
    g_nodes[index].res.vt.draw_ctx = desc->draw_ctx;
    xe_scene_node node = { .hnd = xe_res_handle_gen(g_nodes[index].res.version, index)};
    xe_scene_tr_init(node, desc->pos_x, desc->pos_y, desc->pos_z, desc->scale);
    return node;
}

int xe_drawable_draw(lu_mat4 *tr, void *draw_ctx)
{
    if (!draw_ctx) {
        XE_LOG_ERR("draw_ctx = NULL, ignoring xe_drawable_draw call.");
        return XE_ERR_ARG;
    }
    struct xe_graph_drawable *node = draw_ctx;
    node->mesh = xe_rend_mesh_add(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                                        QUAD_INDICES,  sizeof(QUAD_INDICES));

    int draw_id = xe_rend_material_add((xe_rend_material){.model = *tr, .color = LU_VEC(1.0f, 1.0f, 1.0f, 1.0f), .darkcolor = LU_VEC(0.0f, 0.0f, 0.0f, 1.0f), .tex = xe_image_ptr(node->img)->tex, .pma = 0});
    xe_rend_submit(node->mesh, draw_id);
    return XE_OK;
}

xe_scene_node xe_scene_create_drawable(xe_scene_node_desc *desc, xe_image img, xe_rend_mesh mesh)
{
    struct xe_graph_drawable *node = &g_drawables[g_drawable_count++];
    node->img = img;
    node->mesh = mesh;
    desc->draw_fn = xe_drawable_draw;
    desc->draw_ctx = node;
    node->node = xe_scene_create_node(desc);
    return node->node;
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
    return lu_mat4_rotation_axis(tr, &axis.x, rad);
}

typedef struct xe_scene_iter_state_t {
    int remaining_children;
    int parent_index;
    lu_mat4 trw;
} xe_scene_iter_state_t;

typedef struct xe_scene_iter_stack_t {
    xe_scene_iter_state_t buf[XE_CFG_MAX_SCENE_GRAPH_DEPTH];
    size_t count;
} xe_scene_iter_stack_t;

static inline const xe_scene_iter_state_t*
xe_scene_get_state(xe_scene_iter_stack_t* stack)
{
    static const xe_scene_iter_state_t EMPTY = {
        .remaining_children = 0,
        .parent_index = -1,
        .trw = {.m = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f }}
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
    assert(stack->count < XE_CFG_MAX_SCENE_GRAPH_DEPTH);
}

static inline bool xe_scene_step_state(xe_scene_iter_stack_t* stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

struct xe_draw_task {
    lu_mat4 model;
    int (*func)(lu_mat4 *, void *);
    void *ctx;
};

void xe_scene_draw(void)
{
    static struct xe_draw_task draws[XE_CFG_MAX_ENTITIES];
    static const struct xe_draw_task *DRAWS_END_IT = draws + XE_CFG_MAX_ENTITIES;
    struct xe_draw_task *dt_wit = draws; /* draw task write iterator */
    xe_scene_iter_stack_t state = {
        .buf = {{
            .remaining_children = g_node_count,
            .parent_index = -1,
            .trw = { .m = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }}
        }},
        .count = 1
    };

    for (int i = 0; i < g_node_count; ++i) {
        struct xe_graph_node *node = g_nodes + i;
        if (node->res.vt.draw) {
            assert(dt_wit < DRAWS_END_IT);
            dt_wit->func = node->res.vt.draw;
            dt_wit->ctx = node->res.vt.draw_ctx;
            const xe_scene_iter_state_t* curr = xe_scene_get_state(&state);
            lu_mat4_multiply(dt_wit->model.m, curr->trw.m, g_transforms[node->transform_index].m);
            ++dt_wit;
        }

        if (node->child_count > 0) {
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
    }

    for (struct xe_draw_task *task = draws; task < dt_wit; ++task) {
        task->func(&task->model, task->ctx);
    }
}
