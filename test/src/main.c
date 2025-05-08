#include "xe_platform.h"
#include "xe_renderer.h"
#include "xe_scene.h"

#include "xe_spine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <assert.h>

int main(int argc, char **argv)
{
    xe_platform *plat = xe_platform_create(&(xe_platform_config){
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
 
    xe_scene_node owl = xe_spine_create("./assets/owl-pma.atlas", "./assets/owl.json", 0.03f, "idle");
    xe_scene_node windmill = xe_spine_create("./assets/windmill-pma.atlas", "./assets/windmill.json", 0.025f, "animation");
    xe_scene_node coin = xe_spine_create("./assets/coin-pma.atlas", "./assets/coin-pro.json", 0.05f, "animation");
    xe_transform_translate(windmill, 0.0f, -10.0f, 0.0f);
    

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
        nodes[i] = xe_scene_create_drawable(&desc, tex_test[i], (xe_rend_mesh){});
    }

    /* owl init */
    spAnimationState_setAnimationByName(xe_spine_get_anim(owl), 1, "blink", true);
    spTrackEntry *left = spAnimationState_setAnimationByName(xe_spine_get_anim(owl), 2, "left", true);
    spTrackEntry *right = spAnimationState_setAnimationByName(xe_spine_get_anim(owl), 3, "right", true);
    spTrackEntry *up = spAnimationState_setAnimationByName(xe_spine_get_anim(owl), 4, "up", true);
    spTrackEntry *down = spAnimationState_setAnimationByName(xe_spine_get_anim(owl), 5, "down", true);
	left->alpha = 0;
	left->mixBlend = SP_MIX_BLEND_ADD;
	right->alpha = 0;
	right->mixBlend = SP_MIX_BLEND_ADD;
	up->alpha = 0;
	up->mixBlend = SP_MIX_BLEND_ADD;
	down->alpha = 0;
	down->mixBlend = SP_MIX_BLEND_ADD;

    elapsed = lu_time_elapsed(timer);
    plat->timers_data.scene_load = elapsed;

    plat->timers_data.init_time = lu_time_elapsed(plat->begin_timestamp);
    float deltasec = xe_platform_update();

    while(!plat->close) {
        /* owl update */
        float x = plat->mouse_x / plat->config.display_w;
        float y = plat->mouse_y / plat->config.display_h;
        left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
        right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
        down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
        up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;
        spSkeleton_setToSetupPose(xe_spine_get_skel(owl));

        /* nodes update */
        float curr_time_sec = lu_time_sec(lu_time_elapsed(plat->begin_timestamp));
        xe_transform_set_scale(nodes[0], 1.0f + sinf(curr_time_sec), 1.0f + cosf(curr_time_sec) * 2.0f, 1.0f);
        xe_transform_set_rotation_z(nodes[2], curr_time_sec);

        /* Systems */
        xe_spine_animation_pass(deltasec);
        xe_rend_sync();
        xe_scene_draw();

        deltasec = xe_platform_update();
    }

    xe_platform_shutdown();

    return 0;
}
