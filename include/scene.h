
#define __XE_SCENE_H__
#ifndef __XE_SCENE_H__
#define __XE_SCENE_H__

#include <llulu/lu_str.h>
#include <stdint.h>

typedef struct xe_image_desc {
    const lu_sstr id;
    const char * const img_path;
} xe_image_desc;

typedef struct xe_shader_desc {
    const lu_sstr id;
    const char * const vert_path;
    const char * const frag_path;
} xe_shader_desc;

typedef struct xe_spine_desc {
    const lu_sstr id;
    const char * const atlas_path;
    const char * const skeleton_path;
} xe_spine_desc;

/* Do not combine them like flags. The values are for mask checking from systems.
   e.g. the render system could filter nodes using:
       enum { DRAWABLE_MASK = XE_NODE_QUAD | XE_NODE_SKELETON };
       if (node.type & DRAWABLE_MASK) { drawlist.push(node); }
 */
enum {
    XE_NODE_EMPTY    = 0x0000,
    XE_NODE_SHAPE    = 0x0001,
    XE_NODE_SKELETON = 0x0002,
};

typedef struct xe_scenenode_desc {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation; /* In radians */
    int child_count;
    int type;
    lu_sstr asset_id; // Quad -> Image // Skeleton -> Spine
} xe_scenenode_desc;

typedef struct xe_scene_desc {
    const lu_sstr name;
    struct {
        const xe_image_desc  *images;
        const xe_shader_desc *shaders;
        const xe_spine_desc  *spines;
        const int16_t image_count;
        const int16_t shader_count;
        const int16_t spine_count;
    } assets;
    const xe_scenenode_desc *nodes;
} xe_scene_desc;

extern const xe_image_desc  g_images[];
extern const xe_shader_desc g_shaders[];
extern const xe_spine_desc  g_spines[];
extern const xe_scenenode_desc g_nodes[];
extern const xe_scene_desc scenes[];

#endif /* __XE_SCENE_H__ */