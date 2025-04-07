#include "xe.h"
#include "xe_platform.h"
#include "xe_renderer.h"
#include "xe_scene.h"
#include "spine/AnimationState.h"
#include "spine/Skeleton.h"

#include "xe_spine.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <assert.h>
#include <math.h>

static const xe_rend_vtx QUAD_VERTICES[] = {
    { .x = -1.0f, .y = -1.0f,
      .u = 0.0f, .v = 0.0f,
      .color = 0xFFFF00FF },
    { .x = -1.0f, .y = 1.0f,
      .u = 0.0f, .v = 1.0f,
      .color = 0xFF00FFFF },
    { .x = 1.0f, .y = 1.0f,
      .u = 1.0f, .v = 1.0f,
      .color = 0xFFFFFF00 },
    { .x = 1.0f, .y = -1.0f,
      .u = 1.0f, .v = 0.0f,
      .color = 0xFF00FF00 }
};

static const xe_rend_idx QUAD_INDICES[] = { 0, 1, 2, 0, 2, 3 };

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
    /* TODO: Asset map with user-defined paths */
    xe_image windmill_atlas = xe_image_load("./assets/windmill-pma.png", XE_IMG_PREMUL_ALPHA);
    xe_image owl_atlas = xe_image_load("./assets/owl-pma.png", XE_IMG_PREMUL_ALPHA);
    xe_image coin_atlas = xe_image_load("./assets/coin-pma.png", XE_IMG_PREMUL_ALPHA);
    xe_image tex_test0 = xe_image_load("./assets/tex_test_0.png", 0);
    xe_image tex_test1 = xe_image_load("./assets/tex_test_1.png", 0);
    xe_image tex_test2 = xe_image_load("./assets/tex_test_2.png", 0);
    xe_image tex_test3 = xe_image_load("./assets/tex_test_3.png", 0);

    xe_spine owl = xe_spine_load("./assets/owl-pma.atlas", "./assets/owl.json", 0.01f, "idle");
    xe_spine windmill = xe_spine_load("./assets/windmill-pma.atlas", "./assets/windmill.json", 0.03f, "animation");
    xe_spine coin = xe_spine_load("./assets/coin-pma.atlas", "./assets/coin-pro.json", 0.05f, "idle");
    

    int64_t elapsed = lu_time_elapsed(timer);
    plat->timers_data.img_load = elapsed;
    timer = lu_time_get();

    xe_scene_node nodes[] = {
        { .image = tex_test0 },
        { .image = tex_test1 },
        { .image = tex_test2 },
        { .image = tex_test3 },
    };

    for (int i = 0; i < sizeof(nodes) / sizeof(*nodes); ++i) {
        xe_scene_node *n = nodes + i;
        xe_scene_tr_init(n, 1.0f * i, 2.0f * i, -30.0f, 1.0f);
    }

    /*spAtlas *sp_atlas = spAtlas_createFromFile("./assets/owl-pma.atlas", &owl_atlas);
    spSkeletonJson *skel_json = spSkeletonJson_create(sp_atlas);
    spSkeletonData *skel_data = spSkeletonJson_readSkeletonDataFile(skel_json, "./assets/owl.json");
    if (!skel_data) {
        printf("%s\n", skel_json->error);
        return 1;
    }
    spAnimationStateData *anim_data = spAnimationStateData_create(skel_data);
    xe_scene_spine spine_node = {.skel = spSkeleton_create(skel_data), .anim = spAnimationState_create(anim_data)};
    spine_node.skel->scaleX = 0.01f;
    spine_node.skel->scaleY = 0.01f;
    spSkeleton_setToSetupPose(spine_node.skel);

    spine_node.node.image = owl_atlas;
    spSkeleton_updateWorldTransform(spine_node.skel, SP_PHYSICS_UPDATE);

    xe_scene_tr_init(&spine_node.node, 1.0f, 0.0f, -20.0f, 1.0f);
    spAnimationState_setAnimationByName(spine_node.anim, 0, "idle", true);
    */
    spAnimationState_setAnimationByName(spine_node.anim, 1, "blink", true);

    spTrackEntry *left = spAnimationState_setAnimationByName(spine_node.anim, 2, "left", true);
	spTrackEntry *right = spAnimationState_setAnimationByName(spine_node.anim, 3, "right", true);
	spTrackEntry *up = spAnimationState_setAnimationByName(spine_node.anim, 4, "up", true);
	spTrackEntry *down = spAnimationState_setAnimationByName(spine_node.anim, 5, "down", true);

	left->alpha = 0;
	left->mixBlend = SP_MIX_BLEND_ADD;
	right->alpha = 0;
	right->mixBlend = SP_MIX_BLEND_ADD;
	up->alpha = 0;
	up->mixBlend = SP_MIX_BLEND_ADD;
	down->alpha = 0;
	down->mixBlend = SP_MIX_BLEND_ADD;

    /* Windmill */
    #if 0
    spAtlas *windmill_atlas_file = spAtlas_createFromFile("./assets/windmill-pma.atlas", &windmill_atlas);
    spSkeletonJson *windmill_skel_json = spSkeletonJson_create(windmill_atlas_file);
    spSkeletonData *windmill_skel_data = spSkeletonJson_readSkeletonDataFile(windmill_skel_json, "./assets/windmill.json");
    if (!windmill_skel_data) {
        printf("%s\n", windmill_skel_json->error);
        return 1;
    }
    spAnimationStateData *windmill_anim_data = spAnimationStateData_create(windmill_skel_data);
    xe_scene_spine windmill_spine = {.skel = spSkeleton_create(windmill_skel_data), .anim = spAnimationState_create(windmill_anim_data)};
    windmill_spine.skel->scaleX = 0.03f;
    windmill_spine.skel->scaleY = 0.03f;
    spSkeleton_setToSetupPose(windmill_spine.skel);
    windmill_spine.node.image = windmill_atlas;
    spSkeleton_updateWorldTransform(windmill_spine.skel, SP_PHYSICS_UPDATE);
    xe_scene_tr_init(&windmill_spine.node, 2.0f, 0.0f, -10.0f, 1.0f);
    spAnimationState_setAnimationByName(windmill_spine.anim, 0, "animation", true);
    #endif

    /* Coin */
    #if 0
    spAtlas *coin_atlas_file = spAtlas_createFromFile("./assets/coin-pma.atlas", &coin_atlas);
    spSkeletonJson *coin_skel_json = spSkeletonJson_create(coin_atlas_file);
    spSkeletonData *coin_skel_data = spSkeletonJson_readSkeletonDataFile(coin_skel_json, "./assets/coin-pro.json");
    if (!coin_skel_data) {
        XE_LOG_ERR("%s.", coin_skel_json->error);
        return 1;
    }
    spAnimationStateData *coin_anim_data = spAnimationStateData_create(coin_skel_data);
    xe_scene_spine coin_spine = {.skel = spSkeleton_create(coin_skel_data), .anim = spAnimationState_create(coin_anim_data)};
    coin_spine.skel->scaleX = 0.05f;
    coin_spine.skel->scaleY = 0.05f;
    coin_spine.node.image = coin_atlas;
    spSkeleton_updateWorldTransform(coin_spine.skel, SP_PHYSICS_UPDATE);
    xe_scene_tr_init(&coin_spine.node, 4.0f, -2.0f, -25.0f, 1.0f);
    spAnimationState_setAnimationByName(coin_spine.anim, 0, "animation", true);
    spSkeleton_setToSetupPose(coin_spine.skel);
    #endif

    elapsed = lu_time_elapsed(timer);
    plat->timers_data.scene_load = elapsed;

    plat->timers_data.init_time = lu_time_elapsed(plat->begin_timestamp);
    float deltasec = xe_platform_update();

    while(!plat->close) {
        float x = plat->mouse_x / plat->config.display_w;
        float y = plat->mouse_y / plat->config.display_h;
        left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
        right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
        down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
        up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;

        spSkeleton_setToSetupPose(spine_node.skel);
        //xe_scene_spine_update(&spine_node, deltasec);
        //xe_scene_spine_update(&windmill_spine, deltasec);
        //xe_scene_spine_update(&coin_spine, deltasec);
        xe_spine_animation_pass(deltasec);

        /*
        float curr_time_sec = lu_time_sec(lu_time_elapsed(plat->begin_timestamp));
        const float sin_time = sinf(curr_time_sec);
        nodes[0].scale_x = 0.5f + sin_time;
        nodes[0].scale_y = cosf(curr_time_sec) * 2.0f;
        nodes[2].rotation = curr_time_sec;
        */

        xe_rend_sync();
        xe_spine_draw_pass();
        //xe_scene_spine_draw(&windmill_spine);
        xe_rend_mesh mesh = xe_rend_mesh_add(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                                           QUAD_INDICES,  sizeof(QUAD_INDICES));
        for (int i = 0; i < 4; ++i) {
            nodes[i].mesh = mesh;
            xe_scene_node_draw(&nodes[i]);
        }

        //xe_scene_spine_draw(&spine_node);
        //xe_scene_spine_draw(&coin_spine);

        deltasec = xe_platform_update();
    }

    xe_platform_shutdown();

    return 0;
}
