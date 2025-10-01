#include "xe_nuklear.h"
#include <xe_scene.h>
#include <xe_platform.h>
#include <../src/xe_scene_internal.h>
#include <xe_gfx.h>
#include <../src/xe_gfx_internal.h>
#include <llulu/lu_defs.h>
#include <llulu/lu_error.h>
#include <llulu/lu_math.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_DEFAULT_ALLOCATOR     /*  TODO: remove */
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_FONT_BAKING
//#define NK_IMPLEMENTATION
#include <nuklear.h>

#include <limits.h>
#include <time.h>
#include <string.h>

typedef struct xe_nuklear {
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    xe_platform *plat;
    xe_image default_img;
    struct nk_draw_null_texture tex_null;
} xe_nuklear;

static xe_nuklear g_nuk;

void
xe_nk_init(struct xe_platform *plat)
{
    lu_err_assert(!g_nuk.plat && "Nuklear state should be uninitialized");
    g_nuk.plat = plat;
    nk_init_default(&g_nuk.ctx, 0);
    nk_font_atlas_init_default(&g_nuk.atlas);
    nk_font_atlas_begin(&g_nuk.atlas);
    struct nk_font *karla = nk_font_atlas_add_from_file(&g_nuk.atlas, "./assets/fonts/Karla-Regular.ttf", 18, 0);

    const void *img_data;
    int w, h;
    img_data = nk_font_atlas_bake(&g_nuk.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    //((unsigned int*)img_data)[0] = 0xFFFFFFFF;
    lu_err_assert(img_data);
    xe_image img = xe_image_load_data((void*)img_data, w, h, 4, 0);
    nk_font_atlas_end(&g_nuk.atlas, nk_handle_id(img.id), &g_nuk.tex_null);
    /*
    if (g_nuk.atlas.default_font) {
        nk_style_set_font(&g_nuk.ctx, &g_nuk.atlas.default_font->handle);
    }
    */
    //nk_style_load_all_cursors(&g_nuk.ctx, g_nuk.atlas.cursors);
    nk_style_set_font(&g_nuk.ctx, &karla->handle);

    /*
    static uint32_t nuklear_default_image_data[64] = { 0xFFFFFFFF };
    g_nuk.default_img = xe_image_load_data(nuklear_default_image_data, 8, 8, 4, 0);
    nk_font_atlas_cleanup(&g_nuk.atlas);
    */
}

struct nk_context *
xe_nk_new_frame(void)
{
    xe_platform *plat = g_nuk.plat;
    nk_input_begin(&g_nuk.ctx);

    nk_input_motion(&g_nuk.ctx, plat->mouse_x, plat->mouse_y);
    nk_input_button(&g_nuk.ctx, NK_BUTTON_LEFT,
            plat->mouse_x, plat->mouse_y, plat->mouse_left);
    nk_input_button(&g_nuk.ctx, NK_BUTTON_RIGHT,
            plat->mouse_x, plat->mouse_y, plat->mouse_right);

    nk_input_end(&g_nuk.ctx);
    return &g_nuk.ctx;
}

void
xe_nk_render(void)
{
    static const struct nk_draw_vertex_layout_element vertex_layout[] = {
        {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(xe_gfx_vtx, x)},
        {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(xe_gfx_vtx, u)},
        {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(xe_gfx_vtx, color)},
        {NK_VERTEX_LAYOUT_END}
    };

    struct nk_buffer cmd_buf;
    struct nk_buffer vtx_buf;
    struct nk_buffer idx_buf;
    struct nk_convert_config config = {0};
    memset(&config, 0, sizeof(config));
    config.vertex_layout = vertex_layout;
    config.vertex_size = sizeof(xe_gfx_vtx);
    config.vertex_alignment = __alignof(xe_gfx_vtx);
    config.tex_null = g_nuk.tex_null;
    config.circle_segment_count = 22;
    config.curve_segment_count = 22;
    config.arc_segment_count = 22;
    config.global_alpha = 1.0f;
    config.shape_AA = NK_ANTI_ALIASING_ON;
    config.line_AA = NK_ANTI_ALIASING_ON;

    nk_buffer_init_default(&cmd_buf);
    nk_buffer_init_default(&vtx_buf);
    nk_buffer_init_default(&idx_buf);
    nk_convert(&g_nuk.ctx, &cmd_buf, &vtx_buf, &idx_buf, &config);
    xe_mesh mesh =
        xe_mesh_add(nk_buffer_memory(&vtx_buf),
        nk_buffer_total(&vtx_buf),
        nk_buffer_memory(&idx_buf),
        nk_buffer_total(&idx_buf));

    int first_index = mesh.first_idx;
    const struct nk_draw_command *cmd = NULL;

    lu_mat4 inv_vp;
    lu_mat4_inverse(inv_vp.m, view_projection.m);

    lu_mat4 ui_vp = LU_MAT4_IDENTITY;
    ui_vp.m[0] = 2.0f / (float)g_nuk.plat->viewport_w;
    ui_vp.m[5] = -2.0f / (float)g_nuk.plat->viewport_h;
    ui_vp.m[10] = -1.0f;
    ui_vp.m[12] = -1.0f;
    ui_vp.m[13] = 1.0f;

    lu_mat4_multiply(ui_vp.m, inv_vp.m, ui_vp.m);

    nk_draw_foreach(cmd, &g_nuk.ctx, &cmd_buf) {
        if (!cmd->elem_count) {
            continue;
        }
        xe_gfx_rops_set((xe_gfx_rops){
            .clip = {
                .x = cmd->clip_rect.x > 0.0f ? (uint16_t)cmd->clip_rect.x : 0,
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
    nk_buffer_free(&cmd_buf);
    nk_buffer_free(&vtx_buf);
    nk_buffer_free(&idx_buf);
}

void
xe_nk_shutdown(void)
{
    nk_font_atlas_clear(&g_nuk.atlas);
    nk_free(&g_nuk.ctx);
}

