#include <xe_platform.h>
#include <xe_gfx.h>
#include <xe_scene.h>
#include <../src/xe_scene_internal.h>
#include <llulu/lu_defs.h>
#include <llulu/lu_math.h>

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

struct {
    lu_mat4 tr;
    xe_image tex;
} hello_node;

int main(int argc, char **argv)
{
    xe_platform *plat = xe_platform_create(&(xe_platform_config){
        .title = "Xee world!",
        .display_w = 600,
        .display_h = 400,
        .vsync = true,
        .log_filename = ""
    });

    if (!plat) {
        return 1;
    }

    hello_node.tex = xe_image_load("./assets/tex_test_3.png", 0);
    lu_mat4_identity((float*)&hello_node.tr);
    /* scale */
    hello_node.tr.m[0] =  0.5f;
    hello_node.tr.m[5] =  0.5f;
    hello_node.tr.m[10] =  1.0f;
    hello_node.tr.m[15] =  1.0f;

    float deltasec = xe_platform_update();
    while(!plat->close) {
        static float offset = 0.0f;
        /* (x, y) pos */
        hello_node.tr.m[12] = lu_sin(offset);
        hello_node.tr.m[13] = lu_cos(offset);
        offset += deltasec * 2.0f;

        xe_gfx_sync();
        xe_gfx_material material = (xe_gfx_material) { 
            .model = hello_node.tr,
            .color = LU_VEC(1.0f, 1.0f, 1.0f, 1.0f),
            .darkcolor = LU_VEC(0.0f, 0.0f, 0.0f, 1.0f),
            .tex = xe_image_ptr(hello_node.tex)->tex,
            .pma = 0
        };

        xe_gfx_push(QUAD_VERTICES, sizeof(QUAD_VERTICES), QUAD_INDICES, sizeof(QUAD_INDICES), &material);
        deltasec = xe_platform_update();
    }
    xe_platform_shutdown();

    return 0;
}

