#include "scene.h"

const orx_image_desc g_images[] = {
    { .id = {"owl"}, .img_path = "./assets/owl.png"},
    { .id = {"default"}, .img_path = "./assets/default_tex.png"},
    { .id = {"test"}, .img_path = "./assets/tex_test.png"},
};

const orx_shader_desc g_shaders[] = {
    { .id = {"generic"}, .vert_path = "./assets/vert.glsl", .frag_path = "./assets/frag.glsl" },
};

const orx_spine_desc g_spines[] = {
    { .id = {"owl"}, .atlas_path = "./assets/owl.atlas", .skeleton_path = "./assets/owl-pro.json" },
};

const orx_scenenode_desc g_nodes[] = {
    { 
        .pos_x = 0.0f, .pos_y = 0.0f, .scale_x = 1.0f, .scale_y = 1.0f, .rotation = 0.0f,
        .type = ORX_SNT_SPINE, .asset_id = {"owl"}, .node = {
            .self = 0, .first_child = -1, .next_sibling = -1, .parent = -1
        }
    }
};

enum {
    ORX_ASSETS_IMG_COUNT = sizeof(g_images) / sizeof(g_images[0]),
    ORX_ASSETS_SHADER_COUNT = sizeof(g_shaders) / sizeof(g_shaders[0]),
    ORX_ASSETS_SPINE_COUNT = sizeof(g_spines) / sizeof(g_spines[0]),
    ORX_SCENE_NODE_COUNT = sizeof(g_nodes) / sizeof(g_nodes[0])
};
