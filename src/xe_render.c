
#include "xe_render.h"

enum {
    XE_RENDER_PASS_POOL_LIMIT = 1,
    XE_RENDER_PASS_BATCH_LIMIT = 512,

    XE_RENDER_PASS_INVALID_REF = -1,
};

typedef struct xe_render_pass_ops xe__ops;

typedef struct xe_render_batch {
    uint16_t start_offset;
    uint16_t cmd_count;
    xe__ops ops;
} xe__batch;

typedef struct xe_render_pass {
    lu_rect viewport;
    lu_color background;
    bool clear_color;
    bool clear_depth;
    bool clear_stencil;
    uint16_t head_batch;
    xe__batch batches[XE_RENDER_PASS_BATCH_LIMIT];
} xe__rp;

typedef struct xe_render_context {
    xe__rp rp_pool[XE_RENDER_PASS_POOL_LIMIT];
    uint16_t rp_next;
    bool rp_active[XE_RENDER_PASS_POOL_LIMIT];

    xe_render_pipeline_ref pipeline;
} xe__ctx;

static xe__ctx ctx;

xe_render_pass_ref
xe_render_pass_grab(void)
{
    for (int i = 0; i < XE_RENDER_PASS_POOL_LIMIT; ++i) {
        if (!ctx.rp_active[i]) {
            ctx.rp_active[i] = true;
            return (xe_render_pass_ref){ .handle = i };
        }
    }

    return (xe_render_pass_ref){ .handle = -1 };
}

