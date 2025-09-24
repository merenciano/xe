#include <xe_platform.h>
#include <xe_gfx.h>
#include <llulu/lu_math.h>
#include <stdio.h>
#include <stdint.h>

struct xe_dimensions { uint16_t width, height; };

typedef int xe_easel_ref;
xe_easel_ref xe_easel_setup(int w, int h, void*(*toolbox)(const char*));

void xe_easel_reveal(xe_easel_ref);
struct xe_dimensions xe_easel_dimensions(xe_easel_ref);

typedef int xe_canvas_ref; // framebuffer
xe_canvas_ref xe_canvas_setup(int w, int h, lu_color bg_color);

typedef int xe_sample_ref; // image / texture

typedef int xe_brush_ref; // render pass
void xe_brush_drawing_begin(xe_brush_ref, xe_canvas_ref);

typedef int xe_palette_ref; // pipeline
xe_palette_ref xe_palette_setup(const char *vert_path, const char *frag_path);
xe_sample_ref xe_palette_add_sample(xe_palette_ref, const char *path);

struct xe_palette_swatch {
    xe_sample_ref sample;
    lu_color color;
    lu_color dark_color;
};

struct xe_stroke_data {
    float *vert;
    ptrdiff_t vert_size;
    xe_gfx_idx *indices;
    ptrdiff_t indices_size;
    lu_mat4 transform;
    struct xe_palette_swatch swatch;
};

xe_brush_ref xe_brush_draw_begin(xe_palette_ref, xe_canvas_ref); /* use pipeline and set target framebuffer */
void xe_brush_cover(xe_brush_ref, lu_color); /* clear */
void xe_brush_set_ruler(xe_brush_ref, lu_rect); /* scissor */
void xe_brush_thicken(xe_brush_ref); /* disable blend i.e. solid */
void xe_brush_dilute(xe_brush_ref, uint32_t blend_flags);   /* mix, multiply, add */
void xe_brush_set_charcoal(xe_brush_ref, uint32_t charcoal_flags); /* depth: read, write, test_func */
void xe_brush_paint(xe_brush_ref, struct xe_stroke_data); /* drawcommand */
void xe_brush_draw_end(xe_brush_ref);

typedef int xe_stroke; // drawcmd

struct {
    xe_platform plat;
    xe_easel_ref easel;
    xe_palette_ref palette;
    xe_canvas_ref canvas;
} g_mock_state;

float xe_appreciate_scene(void) // scene update
{
    return xe_platform_update();
}


int main()
{
    if (!xe_platform_init(&g_mock_state.plat, &(xe_platform_config) {
            .title = "xe mocardo",
            .display_w = 800,
            .display_h = 600,
            .vsync = true })) {
        printf("Could not initialize platform context. Aborting...\n");
        return 1;
    }

    g_mock_state.easel = xe_easel_setup(
            g_mock_state.plat.viewport_w,
            g_mock_state.plat.viewport_h,
            g_mock_state.plat.gl_loader);

    struct xe_dimensions easel_size = xe_easel_dimensions(g_mock_state.easel);
    g_mock_state.canvas = xe_canvas_setup(
            easel_size.width,
            easel_size.height,
            (lu_color){0.8f, 0.8f, 0.8f, 1.0f});

    g_mock_state.palette = xe_palette_setup("./assets/vert.glsl", "./assets/frag.glsl");
    xe_palette_add_sample(g_mock_state.palette, "./assets/default.png");


    float breath_duration = xe_appreciate_scene();
    while (!g_mock_state.plat.close) {
        xe_brush_ref brush = xe_easel_grab_brush(g_mock_state.easel);


        xe_easel_reveal(g_mock_state.easel);
        breath_duration = xe_appreciate_scene();
    }






    return 0;
}

