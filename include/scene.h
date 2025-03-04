#ifndef __ORX_SCENE_H__
#define __ORX_SCENE_H__

#include <llulu/lu_str.h>
#include <stdint.h>

typedef struct orx_image_desc {
    const lu_sstr id;
    const char * const img_path;
} orx_image_desc;

typedef struct orx_shader_desc {
    const lu_sstr id;
    const char * const vert_path;
    const char * const frag_path;
} orx_shader_desc;

typedef struct orx_spine_desc {
    const lu_sstr id;
    const char * const atlas_path;
    const char * const skeleton_path;
} orx_spine_desc;

enum {
    ORX_SNT_EMPTY,
    ORX_SNT_QUAD,
    ORX_SNT_SPINE,
};

typedef struct orx_scenenode_desc {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation; /* In radians */
    int type;
    lu_sstr asset_id; // Quad -> Image // Skeleton -> Spine
    int child_count;
} orx_scenenode_desc;

typedef struct orx_scene_desc {
    const lu_sstr name;
    struct {
        const orx_image_desc  *images;
        const orx_shader_desc *shaders;
        const orx_spine_desc  *spines;
        const int16_t image_count;
        const int16_t shader_count;
        const int16_t spine_count;
    } assets;
    const orx_scenenode_desc *nodes;
} orx_scene_desc;

extern const orx_image_desc  g_images[];
extern const orx_shader_desc g_shaders[];
extern const orx_spine_desc  g_spines[];
extern const orx_scenenode_desc g_nodes[];
extern const orx_scene_desc scenes[];

#endif /* __ORX_SCENE_H__ */