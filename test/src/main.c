#include "xe_platform.h"
#include "xe_renderer.h"
#include "xe_scene.h"
#include "spine/AnimationState.h"
#include "spine/Skeleton.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <stdio.h>
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

    lu_timestamp timer = lu_time_get();
    /* TODO: Asset map with user-defined paths */
    xe_rend_img owl_atlas = xe_rend_img_load("./assets/owl.png");
    xe_rend_img windmill_atlas = xe_rend_img_load("./assets/windmill.png");
    xe_rend_img tex_test0 = xe_rend_img_load("./assets/tex_test_0.png");
    xe_rend_img tex_test1 = xe_rend_img_load("./assets/tex_test_1.png");
    xe_rend_img tex_test2 = xe_rend_img_load("./assets/tex_test_2.png");
    xe_rend_img tex_test3 = xe_rend_img_load("./assets/tex_test_3.png");

    xe_platform *plat = xe_platform_create(&(xe_platform_config){
        .title = "XE TEST",
        .display_w = 2280,
        .display_h = 1860,
        .vsync = false,
        .log_filename = ""
    });

    if (!plat) {
        return 1;
    }

    xe_rend_tex_set(owl_atlas.tex, owl_atlas.data);
    stbi_image_free(owl_atlas.data);
    owl_atlas.data = NULL;

    xe_rend_tex_set(windmill_atlas.tex, windmill_atlas.data);
    stbi_image_free(windmill_atlas.data);
    windmill_atlas.data = NULL;

    xe_rend_tex_set(tex_test0.tex, tex_test0.data);
    stbi_image_free(tex_test0.data);
    tex_test0.data = NULL;

    xe_rend_tex_set(tex_test1.tex, tex_test1.data);
    stbi_image_free(tex_test1.data);
    tex_test1.data = NULL;

    xe_rend_tex_set(tex_test2.tex, tex_test2.data);
    stbi_image_free(tex_test2.data);
    tex_test2.data = NULL;

    xe_rend_tex_set(tex_test3.tex, tex_test3.data);
    stbi_image_free(tex_test3.data);
    tex_test3.data = NULL;

    int64_t elapsed = lu_time_elapsed(timer);
    plat->timers_data.img_load = elapsed;
    timer = lu_time_get();

    xe_scene_node nodes[] = {
        {
            .pos_x = -10.5f,
            .pos_y = -10.0f,
            .scale_x = 200.0f,
            .scale_y = 200.0f,
            .rotation = 0.0f,
            .tex = tex_test0.tex,
#ifdef XE_DEBUG
            .name = "Node0"
#endif
        },
        {
            .pos_x = 30.0f,
            .pos_y = 60.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test1.tex,
#ifdef XE_DEBUG
            .name = "Node1"
#endif
        },
        {
            .pos_x = 600.0f,
            .pos_y = 400.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test2.tex,
#ifdef XE_DEBUG
            .name = "Node2"
#endif
        },
        {
            .pos_x = -200.0f,
            .pos_y = 200.0f,
            .scale_x = 100.0f,
            .scale_y = 100.0f,
            .rotation = 0.0f,
            .tex = tex_test3.tex,
#ifdef XE_DEBUG
            .name = "Node3"
#endif
        },
    };

    spBone_setYDown(-1);
    spAtlas *sp_atlas = spAtlas_createFromFile("./assets/owl.atlas", &owl_atlas);
    spSkeletonJson *skel_json = spSkeletonJson_create(sp_atlas);
    spSkeletonData *skel_data = spSkeletonJson_readSkeletonDataFile(skel_json, "./assets/owl.json");
    if (!skel_data) {
        printf("%s\n", skel_json->error);
        return 1;
    }
    spAnimationStateData *anim_data = spAnimationStateData_create(skel_data);
    xe_scene_spine spine_node = {.skel = spSkeleton_create(skel_data), .anim = spAnimationState_create(anim_data)};
    spSkeleton_setToSetupPose(spine_node.skel);

    spine_node.node.tex = owl_atlas.tex;
    spSkeleton_updateWorldTransform(spine_node.skel, SP_PHYSICS_UPDATE);

    spine_node.node.pos_x = 400.0f;
    spine_node.node.pos_y = 400.0f;
    spine_node.node.scale_x = 0.6f;
    spine_node.node.scale_y = 0.6f;
    spine_node.node.rotation = 0.0f;
    spAnimationState_setAnimationByName(spine_node.anim, 0, "idle", true);
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
    spAtlas *windmill_atlas_file = spAtlas_createFromFile("./assets/windmill.atlas", &windmill_atlas);
    spSkeletonJson *windmill_skel_json = spSkeletonJson_create(windmill_atlas_file);
    spSkeletonData *windmill_skel_data = spSkeletonJson_readSkeletonDataFile(windmill_skel_json, "./assets/windmill.json");
    if (!windmill_skel_data) {
        printf("%s\n", windmill_skel_json->error);
        return 1;
    }
    spAnimationStateData *windmill_anim_data = spAnimationStateData_create(windmill_skel_data);
    xe_scene_spine windmill_spine = {.skel = spSkeleton_create(windmill_skel_data), .anim = spAnimationState_create(windmill_anim_data)};
    spSkeleton_setToSetupPose(windmill_spine.skel);
    windmill_spine.node.tex = windmill_atlas.tex;
    spSkeleton_updateWorldTransform(windmill_spine.skel, SP_PHYSICS_UPDATE);
    windmill_spine.node.pos_x = -400.0f;
    windmill_spine.node.pos_y = -400.0f;
    windmill_spine.node.scale_x = 0.4f;
    windmill_spine.node.scale_y = 0.4f;
    windmill_spine.node.rotation = 0.0f;
    spAnimationState_setAnimationByName(windmill_spine.anim, 0, "animation", true);

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
        xe_scene_spine_update(&spine_node, deltasec);
        xe_scene_spine_update(&windmill_spine, deltasec);

        float curr_time_sec = lu_time_sec(lu_time_elapsed(plat->begin_timestamp));
        const float sin_time = sinf(curr_time_sec);
        nodes[0].scale_x = 100.0f + sin_time * 50.0f;
        nodes[0].scale_y = 100.0f + cosf(curr_time_sec) * 50.0f;
        nodes[2].rotation = curr_time_sec;

        xe_rend_sync();
        xe_scene_spine_draw(&windmill_spine);
        xe_rend_mesh mesh = xe_rend_mesh_add(QUAD_VERTICES, sizeof(QUAD_VERTICES),
                                           QUAD_INDICES,  sizeof(QUAD_INDICES));
        for (int i = 0; i < 4; ++i) {
            xe_rend_draw_id draw_index = xe_rend_material_add((xe_rend_material){
                .apx = nodes[i].pos_x,
                .apy = nodes[i].pos_y,
                .asx = nodes[i].scale_x,
                .asy = nodes[i].scale_y,
                .arot = nodes[i].rotation,
                .tex = nodes[i].tex
            });
            xe_rend_submit(mesh, draw_index);
        }

        xe_scene_spine_draw(&spine_node);

        deltasec = xe_platform_update();
    }

    xe_platform_shutdown();

    return 0;
}
