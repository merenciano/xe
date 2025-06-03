#include "xe_platform.h"
#include "xe_renderer.h"
#include "xe_scene.h"

#include "xe_spine.h"

#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>

xe_platform *plat = NULL;

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
    float x = plat->mouse_x / plat->config.display_w;
    float y = plat->mouse_y / plat->config.display_h;
    tracks->left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
    tracks->right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
    tracks->down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
    tracks->up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;
    spSkeleton_setToSetupPose(xe_spine_get_skel(self));
}

int main(int argc, char **argv)
{
    plat = xe_platform_create(&(xe_platform_config){
        .title = "XE TEST",
        .display_w = 1200,
        .display_h = 900,
        .vsync = false,
        .log_filename = ""
    });

    if (!plat) {
        return 1;
    }

    lu_timestamp timer = lu_time_get();

    xe_image tex_test[] = {
        xe_image_load("./assets/tex_test_0.png", 0),
        xe_image_load("./assets/tex_test_1.png", 0),
        xe_image_load("./assets/tex_test_2.png", 0),
        xe_image_load("./assets/tex_test_3.png", 0)
    };
 
    owl_tracks owltracks;
    xe_scene_node owl = xe_spine_create("./assets/owl-pma.atlas", "./assets/owl.json", 0.03f, "idle");
    xe_scene_node windmill = xe_spine_create("./assets/windmill-pma.atlas", "./assets/windmill.json", 0.025f, "animation");
    xe_scene_node coin = xe_spine_create("./assets/coin-pma.atlas", "./assets/coin-pro.json", 0.05f, "animation");
    xe_transform_translate(windmill, 0.0f, -10.0f, 0.0f);

    owl_init(owl, &owltracks);

    int64_t elapsed = lu_time_elapsed(timer);
    plat->timers_data.img_load = elapsed;
    timer = lu_time_get();

    xe_scene_node nodes[4];
    for (int i = 0; i < sizeof(nodes) / sizeof(*nodes); ++i) {
        xe_scene_node_desc desc = {
            .pos_x = 10.0f,
            .pos_y = 10.0f,
            .pos_z = -20.0f,
            .scale = 6.0f - i
        };
        nodes[i] = xe_scene_create_drawable(&desc, tex_test[i]);
    }


    elapsed = lu_time_elapsed(timer);
    plat->timers_data.scene_load = elapsed;

    plat->timers_data.init_time = lu_time_elapsed(plat->begin_timestamp);
    float deltasec = xe_platform_update();

    while(!plat->close) {
        owl_update(owl, &owltracks);

        /* nodes update */
        float curr_time_sec = lu_time_sec(lu_time_elapsed(plat->begin_timestamp));
        xe_transform_set_scale(nodes[0], 1.0f + lu_sin(curr_time_sec), 1.0f + lu_cos(curr_time_sec) * 2.0f, 1.0f);
        xe_transform_set_rotation_z(nodes[2], curr_time_sec);

        /* Systems */
        xe_spine_animation_pass(deltasec);
        xe_rend_sync();
        xe_scene_update_world();
        xe_spine_draw_pass();

        deltasec = xe_platform_update();
    }

    xe_platform_shutdown();

    return 0;
}
