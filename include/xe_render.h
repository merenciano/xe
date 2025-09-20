
#ifndef XE_RENDER_H
#define XE_RENDER_H

#include <llulu/lu_math.h>

typedef struct { int id;     } xe_draw_id;
typedef struct { int handle; } xe_render_pass_ref;
typedef struct { int handle; } xe_render_pipeline_ref;

xe_render_pass_ref xe_render_pass_grab(void);
void xe_render_pass_drop(xe_render_pass_ref self);

struct xe_render_pass_params {
    /* TODO: Framebuffer */
    xe_render_pipeline_ref pipeline;
    lu_rect viewport;
    lu_color background_color;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
};

void xe_render_pass_setup(xe_render_pass_ref self, struct xe_render_pass_params params);

struct xe_mesh {
    const void *vertices;
    size_t vertices_size;
    const void *indices;
    size_t indices_size;
};

struct xe_render_object {
    struct xe_mesh mesh;
    struct xe_material_data *mat_data;
};

void xe_render_pass_add(xe_render_pass_ref self, struct xe_render_object object);

xe_draw_id xe_render_pass_add_mesh(xe_render_pass_ref self, struct xe_mesh *mesh);

/* Returns the indirect command index in the batch. */
int xe_render_pass_add_material_data(xe_render_pass_ref self, xe_draw_id draw_id, struct xe_material_data *mat_data);

enum xe_render_pass_ops_flags {
    XE_RP_OPS_DEPTH_TEST   = 0x0001,
    XE_RP_OPS_STENCIL_TEST = 0x0002,
    XE_RP_OPS_DEPTH_MASK   = 0x0004,
    XE_RP_OPS_STENCIL_MASK = 0x0008,
    XE_RP_OPS_BLEND        = 0x0010,
    XE_RP_OPS_CULL         = 0x0020,
    XE_RP_OPS_SCISSOR      = 0x0040,
    XE_RP_OPS_MULTISAMPLE  = 0x0080
};

enum xe_render_pass_depth_func {
    XE_RP_DEPTH_NOOP,
    XE_RP_DEPTH_LEQUAL,
    XE_RP_DEPTH_LESS,
    XE_RP_DEPTH_COUNT
};

enum xe_render_pass_blend_func {
    XE_RP_BLEND_NOOP,
    XE_RP_BLEND_ONE,
    XE_RP_BLEND_SRC_ALPHA,
    XE_RP_BLEND_ONE_MINUS_SRC_ALPHA,
    XE_RP_BLEND_ZERO,
    XE_RP_BLEND_COUNT
};

enum xe_render_pass_cull_face {
    XE_RP_CULL_NOOP,
    XE_RP_CULL_FRONT,
    XE_RP_CULL_BACK,
    XE_RP_CULL_FRONT_AND_BACK,
    XE_RP_CULL_COUNT
};

struct xe_render_pass_ops {
    uint32_t enable_flags;
    uint32_t disable_flags;
    lu_rect clip_area;
    enum xe_render_pass_depth_func depth_fun;
    enum xe_render_pass_blend_func blend_src;
    enum xe_render_pass_blend_func blend_dst;
    enum xe_render_pass_cull_face  cull_face;
};

void xe_render_pass_set_ops(xe_render_pass_ref self, struct xe_render_pass_ops ops);

void xe_render_pass_submit(xe_render_pass_ref self);

#endif  /* XE_RENDER_H */

