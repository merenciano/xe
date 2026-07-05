
#include <xe_render.h>
#include <xe_platform.h>
#include <llulu/lu_defs.h>
#include <llulu/lu_math.h>
#include <stb/stb_image.h>
#include <stdio.h>

static inline bool load_texture_from_path(xe_tex *tex, const char *path);

static const xe_vtx QUAD_VERTICES[] = {
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

static const xe_vtx_idx QUAD_INDICES[] = { 0, 1, 2, 0, 2, 3 };

static xe_material QUAD_MATERIAL = {
    .data.generic.model = {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},
    .data.generic.color = {1,1,1,1},
    .data.generic.darkcolor = {0,0,0,0},
    .data.generic.pma = 0.0f
};

static xe_platform g_platform;

int main()
{
    if (!xe_platform_init(&g_platform, &(xe_platform_config) {
            .title = "xe hello demo",
            .display_w = 1920,
            .display_h = 1080,
            .vsync = false,
            .log_filename = "" })) {

        printf("Can not init platform.\n");
        return 1;
    }

    if (!xe_render_init(&(xe_renderconf) {
            .gl_loader = g_platform.gl_loader,
            .vert_shader_path = "./assets/vert.glsl",
            .frag_shader_path = "./assets/frag.glsl",
            .default_ops = xe_draw_state_default(XE_DRAW_STATE_DEFAULT_BLEND),
            .background_color = { .r = 0.02f, .g = 0.01f, .b = 0.04f, .a = 1.0f },
            .viewport = { .x = 0, .y = 0, g_platform.viewport_w, g_platform.viewport_h }
        })) {
        printf("Can not init graphics module.\n");
        return 1;
    }

    if (!load_texture_from_path((xe_tex*) & (QUAD_MATERIAL.data.generic.albedo_idx), "./assets/default.png")) {
        printf("Can not load default texture\n");
        return 1;
    }

    xe_platform_update();
    while (!g_platform.close) {
        xe_render_pass_begin(
            (lu_rect){0, 0, g_platform.viewport_w, g_platform.viewport_h},
            (lu_color){ 1.0f, 0.0f, 0.0f, 1.0f},
            true, true, false,
            (xe_draw_state){ 
                .clip = {0,0,0,0},
                .blend_src = XE_BLEND_UNSET,
                .blend_dst = XE_BLEND_UNSET, 
                .depth = XE_DEPTH_UNSET,
                .cull = XE_CULL_UNSET
            }
        );
        xe_render_push(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                    QUAD_INDICES, sizeof(QUAD_INDICES),
                    &QUAD_MATERIAL);

        xe_render_draw();
        xe_platform_update();
    }

    xe_render_shutdown();
    xe_platform_shutdown();

    return 0;
}

static inline bool load_texture_from_path(xe_tex *tex, const char *path)
{
    int w, h, c;
    void *pixels = stbi_load(path, &w, &h, &c, 0);
    if (!pixels) {
        printf("stbi_load failed for file %s.\n", path);
        return false;
    }

    *tex = xe_render_tex_alloc((xe_texfmt){ .width = w, .height = h, .format = c == 4 ? XE_TEX_RGBA : XE_TEX_RGB, .flags = 0});
    xe_render_tex_load(*tex, pixels);
    stbi_image_free(pixels);
    return true;
}

