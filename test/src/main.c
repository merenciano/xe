#include "xe_platform.h"
#include "xe_gfx.h"
#include "xe_scene.h"

#include "xe_spine.h"

#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <stdio.h>

static xe_platform platform;

typedef struct {
    spTrackEntry *left;
    spTrackEntry *right;
    spTrackEntry *up;
    spTrackEntry *down;
} owl_tracks;

static void owl_init(xe_scene_node self, void *ctx)
{
    owl_tracks *tracks = ctx;
    spAnimationState_setAnimationByName(xe_spine_get_anim(self), 1, "blink", true);
    tracks->left = spAnimationState_setAnimationByName(xe_spine_get_anim(self), 2, "left", true);
    tracks->right = spAnimationState_setAnimationByName(xe_spine_get_anim(self), 3, "right", true);
    tracks->up = spAnimationState_setAnimationByName(xe_spine_get_anim(self), 4, "up", true);
    tracks->down = spAnimationState_setAnimationByName(xe_spine_get_anim(self), 5, "down", true);
    tracks->left->alpha = 0;
    tracks->left->mixBlend = SP_MIX_BLEND_ADD;
    tracks->right->alpha = 0;
    tracks->right->mixBlend = SP_MIX_BLEND_ADD;
    tracks->up->alpha = 0;
    tracks->up->mixBlend = SP_MIX_BLEND_ADD;
    tracks->down->alpha = 0;
    tracks->down->mixBlend = SP_MIX_BLEND_ADD;
}

static void owl_update(xe_scene_node self, void *ctx)
{
    owl_tracks *tracks = ctx;
    float x = platform.mouse_x / (float)platform.window_w;
    float y = platform.mouse_y / (float)platform.window_h;
    tracks->left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
    tracks->right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
    tracks->down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
    tracks->up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;
    spSkeleton_setToSetupPose(xe_spine_get_skel(self));
}

static void node1_update(xe_scene_node self, void *data)
{
    float deltasec = *(float*)data;
    const float *tr = xe_transform_get(self);
    float rotation_mat[16];
    lu_mat4_multiply((float*)tr, lu_mat4_rotation_z(lu_mat4_identity(rotation_mat), deltasec * 0.5f), tr);
    xe_transform_rotate(self, (lu_vec3){0.0f, 0.0f, 1.0f}, deltasec * 2.0f);
}

static void node2_update(xe_scene_node self, void *data)
{
    xe_transform_rotate(self, (lu_vec3){0.0f, 0.0f, 1.0f}, *(float*)data);
}

static void node3_update(xe_scene_node self, void *data)
{
    (void)data;
    float curr_time_sec = lu_time_sec(lu_time_elapsed(platform.begin_timestamp));
    xe_transform_set_scale(self, (2.0f + lu_sin(curr_time_sec * 2.0f)) * 0.5f, (2.0f + lu_cos(curr_time_sec * 2.0f)) * 0.5f, 1.0f);
    xe_transform_set_pos(self, 18.0f * lu_sin(curr_time_sec * 0.7f), 13.0f, -20.0f);
}

int main(int argc, char **argv)
{
    if (!xe_platform_init(&platform, &(xe_platform_config){
            .title = "XE TEST",
            .display_w = 1200,
            .display_h = 900,
            .vsync = true,
            .log_filename = "" })) {
        return 1;
    }

    if (!xe_gfx_init(&((xe_gfx_config) {
        .gl_loader = platform.gl_loader,
        .vert_shader_path = "./assets/vert.glsl",
        .frag_shader_path = "./assets/frag.glsl",
        .default_ops = {
            .clip = {0, 0, 0, 0},
            .blend_src = XE_BLEND_ONE,
            .blend_dst = XE_BLEND_ONE_MINUS_SRC_ALPHA,
            .depth = XE_DEPTH_DISABLED,
            .cull = XE_CULL_NONE
        },
        .background_color = { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f },
        .viewport = { .x = 0, .y = 0, platform.viewport_w, platform.viewport_h }
    }))) {
        printf("Can not init graphics module.\n");
        return 1;
    }

    lu_timestamp timer = lu_time_get();

    xe_image tex_test[] = {
        xe_image_load("./assets/tex_test_0.png", 0),
        xe_image_load("./assets/tex_test_1.png", 0),
        xe_image_load("./assets/tex_test_2.png", 0),
        xe_image_load("./assets/default.png", 0)
    };
 
    owl_tracks owltracks;
    xe_scene_node owl = xe_spine_create("./assets/owl-pma.atlas", "./assets/owl.json", 0.03f, "idle");
    xe_scene_node windmill = xe_spine_create("./assets/windmill-pma.atlas", "./assets/windmill.json", 0.025f, "animation");
    xe_scene_node coin = xe_spine_create("./assets/coin-pma.atlas", "./assets/coin-pro.json", 0.05f, "animation");
    xe_transform_translate(owl, -1.0f, -5.0f, 0.0f);
    xe_transform_translate(windmill, 0.0f, -10.0f, 0.0f);
    xe_transform_translate(coin, 18.0f, -13.0f, 0.0f);
    xe_transform_set_scale(coin, 0.25f, 0.25f, 1.0f);

    xe_scene_register_node_update(owl, &owltracks, owl_update);
    owl_init(owl, &owltracks);

    int64_t elapsed = lu_time_elapsed(timer);
    platform.timers_data.img_load = elapsed;
    timer = lu_time_get();

    xe_scene_node nodes[4];
    for (int i = 0; i < sizeof(nodes) / sizeof(*nodes); ++i) {
        xe_scene_node_desc desc = {
            .pos_x = 0.0f,
            .pos_y = 0.0f,
            .pos_z = -20.0f,
            .scale = 1.8f
        };
        nodes[i] = xe_scene_create_drawable(&desc, tex_test[i]);
    }
    xe_transform_translate(nodes[0], 0.0f, 0.0f, 12.0f);
    xe_transform_set_scale(nodes[0], 10.f, 10.f, 1.0f);
    xe_transform_translate(nodes[1], 0.0f, 16.0f, 0.0f);
    xe_transform_set_scale(nodes[2], 4.0f, 4.0f, 1.0f);
    xe_transform_translate(nodes[2], -15.0f, 10.0f, 0.0f);

    float deltasec = 0.0f;
    xe_scene_register_node_update(nodes[1], &deltasec, node1_update);
    xe_scene_register_node_update(nodes[2], &deltasec, node2_update);
    xe_scene_register_node_update(nodes[3], NULL, node3_update);

    elapsed = lu_time_elapsed(timer);
    platform.timers_data.scene_load = elapsed;

    platform.timers_data.init_time = lu_time_elapsed(platform.begin_timestamp);
    deltasec = xe_platform_update();

    while(!platform.close) {
        xe_gfx_pass_begin(
            (lu_rect){0, 0, platform.viewport_w, platform.viewport_h},
            (lu_color){ 0.1f, 0.07f, 0.1f, 1.0f},
            true, true, false,
            (xe_gfx_rops){ 
                .clip = {0,0,0,0},
                .blend_src = XE_BLEND_DISABLED,
                .blend_dst = XE_BLEND_DISABLED,
                .depth = XE_DEPTH_DISABLED,
                .cull = XE_CULL_NONE
            }
        );

        /* Systems */
        xe_spine_animation_pass(deltasec);
        xe_scene_update_world();
        xe_gfx_sync();
        xe_scene_drawable_draw_pass();
        xe_gfx_rops_set((xe_gfx_rops) {
                .clip = {0,0,0,0},
                .blend_src = XE_BLEND_ONE,
                .blend_dst = XE_BLEND_ONE_MINUS_SRC_ALPHA,
                .depth = XE_DEPTH_UNSET,
                .cull = XE_CULL_UNSET });
        xe_spine_draw_pass();

        xe_gfx_render();
        deltasec = xe_platform_update();
    }

    xe_platform_shutdown();

    return 0;
}
