#ifndef XE_SCENE_INTERNAL_H
#define XE_SCENE_INTERNAL_H

#include <xe_asset.h>
#include <xe_scene.h>
#include <xe_render.h>

#include <stdint.h>

static inline uint16_t
xe_handle_index(xe_handle id) { return (uint16_t)(id & 0xFFFF); }

static inline uint16_t
xe_handle_version(xe_handle id) { return (uint16_t)((id >> 16) & 0xFFFF); }

static inline xe_handle
xe_handle_gen(uint16_t ver, uint16_t idx)
{
    return (ver << 16) + idx;
}

typedef struct xe_asset_image {
    xe_asset asset;
    xe_tex tex;
    const char *path;
    const void *data;
    uint16_t w;
    uint16_t h;
    uint16_t c; /* channels */
    uint16_t flags;
} xe_asset_image;

xe_tex xe_image_tex(xe_image image);
const xe_asset_image *xe_asset_image_data(xe_image image);

typedef struct xe_asset_pipeline {
    xe_asset asset;
    const char *vert_path;
    const char *frag_path;
    const char *vert_source;
    const char *frag_source;
    uint32_t id; /*  program in opengl */
} xe_asset_pipeline;

xe_asset xe_asset_pipeline_load_source(const char *vert_path, const char *frag_path);
xe_asset xe_asset_pipeline_compile(xe_pipeline pipeline);
xe_program xe_asset_pipeline_program(xe_pipeline pipeline);
const xe_asset_pipeline *xe_asset_pipeline_data(xe_pipeline pipeline);


#endif /* XE_SCENE_INTERNAL_H */
