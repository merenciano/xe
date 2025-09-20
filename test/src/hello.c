
#include <xe_gfx.h>
#include <xe_platform.h>
#include <llulu/lu_defs.h>
#include <llulu/lu_log.h>
#include <llulu/lu_math.h>
#include <stb/stb_image.h>
#include <stdio.h>

static inline bool load_texture_from_path(xe_gfx_tex *tex, const char *path);

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
static xe_gfx_material QUAD_MATERIAL = { .model = LU_MAT4_IDENTITY, .color = LU_VEC(1,1,1,1), .darkcolor = LU_VEC4_ZERO, .pma = 0.0f };
static xe_platform g_platform;

int main()
{
    if (!xe_platform_init(&g_platform, &(xe_platform_config) {
            .title = "hello demo",
            .display_w = 1024,
            .display_h = 720,
            .vsync = true,
            .log_filename = "" })) {

        printf("Can not init platform.\n");
        return 1;
    }

    if (!xe_gfx_init(&((xe_gfx_config) {
            .gl_loader = g_platform.gl_loader,
            .vert_shader_path = "./assets/vert.glsl",
            .frag_shader_path = "./assets/frag.glsl",
            .default_ops = {
                .enabled_flags = XE_OP_BLEND,
                .blend_src_fn = XE_BLEND_ONE,
                .blend_dst_fn = XE_BLEND_ONE_MINUS_SRC_ALPHA,
                .depth_fn = XE_DEPTH_NOOP,
                .cull_faces = XE_CULL_NOOP,
                .clip = {0, 0, 0, 0}
            }}))) {
        printf("Can not init graphics module.\n");
        return 1;
    }

    if (!load_texture_from_path(&(QUAD_MATERIAL.tex), "./assets/default.png")) {
        printf("Can not load default texture\n");
        return 1;
    }

    xe_platform_update();
    while (!g_platform.close) {
        xe_gfx_push(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                    QUAD_INDICES, sizeof(QUAD_INDICES),
                    &QUAD_MATERIAL);

        xe_gfx_render(g_platform.viewport_w, g_platform.viewport_h);
        xe_platform_update();
    }

    xe_gfx_shutdown();
    xe_platform_shutdown();

    return 0;
}

static inline bool load_texture_from_path(xe_gfx_tex *tex, const char *path)
{
    int w, h, c;
    void *pixels = stbi_load(path, &w, &h, &c, 0);
    if (!pixels) {
        printf("stbi_load failed for file %s.\n", path);
        return false;
    }

    *tex = xe_gfx_tex_alloc((xe_gfx_texfmt){ .width = w, .height = h, .format = c == 4 ? XE_TEX_RGBA : XE_TEX_RGB, .flags = 0});
    xe_gfx_tex_load(*tex, pixels);
    stbi_image_free(pixels);
    return true;
}

