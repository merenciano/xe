#include "xe_nuklear.h"
#define NK_IMPLEMENTATION
#include <nuklear.h>

#include <xe_scene.h>
#include <xe_platform.h>
#include <../src/xe_scene_internal.h>
#include <xe_gfx.h>
#include <../src/xe_gfx_internal.h>
#include <llulu/lu_defs.h>
#include <llulu/lu_error.h>
#include <llulu/lu_math.h>
#include <llulu/lu_log.h>

#include <string.h>

enum {
    XE_NK_ARENA_SIZE = LU_MEGABYTES(16),
    XE_NK_CMDBUF_SIZE = LU_MEGABYTES(1),
    XE_NK_FONT_SIZE = 22,
};

typedef struct xe_nuklear {
    const xe_platform *plat;
    struct nk_font_atlas atlas;
    struct nk_draw_null_texture tex_null;
    struct nk_context ctx;
    char mem_arena[XE_NK_ARENA_SIZE];
} xe_nuklear;

static xe_nuklear g_nuk;

static void *
xe_nk_arena_alloc(nk_handle userdata, void *ptr, nk_size size)
{
    void *new = NULL;
    size_t *arena_offset = userdata.ptr;
    if (*arena_offset + size < XE_NK_ARENA_SIZE) {
        new = &g_nuk.mem_arena[*arena_offset];
        *arena_offset += size;
    } else {
        lu_log_err("Nuklear arena looped: overwrites may occur.");
        new = g_nuk.mem_arena;
        *arena_offset = size;
    }
    return new;
}

static void
xe_nk_arena_free(nk_handle h, void *p) { }

void
xe_nk_init(const xe_platform *plat)
{
    lu_err_assert(!g_nuk.plat && "Nuklear state should be uninitialized");
    g_nuk.plat = plat;

    /* Using the memory arena for font atlas creation before passing to nk_context */
    size_t arena_offset = 0;
    nk_handle userdata = nk_handle_ptr(&arena_offset);
    struct nk_allocator alloc;
    alloc.userdata = userdata;
    alloc.alloc = xe_nk_arena_alloc;
    alloc.free = xe_nk_arena_free;

    nk_font_atlas_init(&g_nuk.atlas, &alloc);
    nk_font_atlas_begin(&g_nuk.atlas);
    struct nk_font *karla = nk_font_atlas_add_from_file(
        &g_nuk.atlas, "./assets/fonts/Karla-Regular.ttf", XE_NK_FONT_SIZE, 0);
    int w, h;
    const void *img_data = nk_font_atlas_bake(&g_nuk.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    lu_err_assert(img_data);
    xe_image img = xe_image_load_data(img_data, w, h, 4, 0);
    nk_font_atlas_end(&g_nuk.atlas, nk_handle_id((int)img.id), &g_nuk.tex_null);
    nk_font_atlas_cleanup(&g_nuk.atlas);
    /* Atlas created, now handing the mem_arena to nk_ctx */
    nk_init_fixed(&g_nuk.ctx, g_nuk.mem_arena, XE_NK_ARENA_SIZE, NULL);
    nk_style_set_font(&g_nuk.ctx, &karla->handle);
#if 0
    nk_style_load_all_cursors(&g_nuk.ctx, g_nuk.atlas.cursors);
    if (g_nuk.atlas.default_font) {
        nk_style_set_font(&g_nuk.ctx, &g_nuk.atlas.default_font->handle);
    }
#endif
}

struct nk_context *
xe_nk_new_frame(void)
{
    const xe_platform *plat = g_nuk.plat;
    nk_input_begin(&g_nuk.ctx);

    nk_input_motion(&g_nuk.ctx, (int)plat->mouse_x, (int)plat->mouse_y);
    nk_input_button(&g_nuk.ctx, NK_BUTTON_LEFT,
            (int)plat->mouse_x, (int)plat->mouse_y, plat->mouse_left);
    nk_input_button(&g_nuk.ctx, NK_BUTTON_RIGHT,
            (int)plat->mouse_x, (int)plat->mouse_y, plat->mouse_right);

    nk_input_end(&g_nuk.ctx);
    return &g_nuk.ctx;
}

static inline void
xe__nk_get_transform(lu_mat4 *out_matrix)
{
    /* projection matrix expected for widget rendering */
    lu_mat4_identity(out_matrix->m);
    out_matrix->m[0] = 2.0f / (float)g_nuk.plat->viewport_w;
    out_matrix->m[5] = -2.0f / (float)g_nuk.plat->viewport_h;
    out_matrix->m[10] = -1.0f;
    out_matrix->m[12] = -1.0f;
    out_matrix->m[13] = 1.0f;

    /* Since I do not want to swap shaders between scene and UI,
     * I need to 'stack' the negation of the scene's view * projection matrix
     * and the projection for the widgets, all in the 'model' transform. */
    lu_mat4 inv_vp;
    lu_mat4_inverse(inv_vp.m, view_projection.m);
    lu_mat4_multiply(out_matrix->m, inv_vp.m, out_matrix->m);
}

void
xe_nk_render(void)
{
    static char cmdbuf_memory[XE_NK_CMDBUF_SIZE];
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(xe_gfx_vtx, x)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(xe_gfx_vtx, u)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(xe_gfx_vtx, color)},
        {NK_VERTEX_LAYOUT_END}
    };

    struct nk_convert_config config = {0};
    memset(&config, 0, sizeof(config));
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(xe_gfx_vtx);
    config.vertex_alignment = NK_ALIGNOF(xe_gfx_vtx);
    config.tex_null = g_nuk.tex_null;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;

    void *vtx_head, *idx_head;
    size_t vtx_size_rem, idx_size_rem, first_vtx, first_idx;
    xe__vtxbuf_remaining(&vtx_head, &vtx_size_rem, &first_vtx, &idx_head, &idx_size_rem, &first_idx);

    struct nk_buffer cmd_buf, vtx_buf, idx_buf;
    nk_buffer_init_fixed(&cmd_buf, cmdbuf_memory, XE_NK_CMDBUF_SIZE);
    nk_buffer_init_fixed(&vtx_buf, vtx_head, vtx_size_rem);
    nk_buffer_init_fixed(&idx_buf, idx_head, idx_size_rem);
    nk_convert(&g_nuk.ctx, &cmd_buf, &vtx_buf, &idx_buf, &config);

    size_t vsize = nk_buffer_total(&vtx_buf);
    size_t isize = nk_buffer_total(&idx_buf);
    xe_mesh mesh = (xe_mesh){
        .base_vtx = (int)first_vtx,
        .first_idx = (int)first_idx,
        .idx_count = isize / sizeof(xe_gfx_idx)
    };
    xe__vtxbuf_push_nocheck(vsize, isize);

    lu_mat4 ui_vp;
    xe__nk_get_transform(&ui_vp);
    int first_index = mesh.first_idx;
    const struct nk_draw_command *cmd = NULL;
    nk_draw_foreach(cmd, &g_nuk.ctx, &cmd_buf) {
        if (!cmd->elem_count) {
            continue;
        }
        xe_gfx_rops_set((xe_gfx_rops){
            .clip = {
                .x = cmd->clip_rect.x > 0.0f ? (uint16_t)cmd->clip_rect.x : 0, /* TODO: use int32 clip values for config */
                .y = cmd->clip_rect.y > 0.0f ? g_nuk.plat->window_h - (uint16_t)(cmd->clip_rect.y + cmd->clip_rect.h) : 0,
                .w = (uint16_t)cmd->clip_rect.w,
                .h = (uint16_t)cmd->clip_rect.h
            },
            .blend_src = XE_BLEND_SRC_ALPHA,
            .blend_dst = XE_BLEND_ONE_MINUS_SRC_ALPHA,
            .depth = XE_DEPTH_DISABLED,
            .cull = XE_CULL_NONE });

        xe_mesh submesh = {
            .base_vtx = mesh.base_vtx,
            .first_idx = first_index,
            .idx_count = (int)cmd->elem_count
        };
        xe_image img = { .id = cmd->texture.id };
        xe_gfx_material mat = (xe_gfx_material) {
            .model = ui_vp,
            .color = LU_VEC(1.0f, 1.0f, 1.0f, 1.0f),
            .darkcolor = LU_VEC(0.0f, 0.0f, 0.0f, 1.0f),
            .tex = xe_image_ptr(img)->tex,
            .pma = 0,
        };
        int draw_id = xe_material_add(&mat);
        xe_drawcmd_add(submesh, draw_id);
        first_index += cmd->elem_count;
    }

    nk_clear(&g_nuk.ctx);
}

void
xe_nk_shutdown(void)
{
    nk_font_atlas_clear(&g_nuk.atlas);
    nk_free(&g_nuk.ctx);
}
