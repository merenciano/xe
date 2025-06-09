#ifndef XE_INTERNAL_SCENE_H
#define XE_INTERNAL_SCENE_H

#include <xe_scene.h>

#include <stdint.h>

static inline uint16_t
xe_res_index(xe_handle id) { return (uint16_t)(id & 0xFFFF); }

static inline uint16_t
xe_res_version(xe_handle id) { return (uint16_t)((id >> 16) & 0xFFFF); }

static inline xe_handle
xe_res_handle_gen(uint16_t ver, uint16_t idx)
{
    return (ver << 16) + idx;
}

enum xe_res_state {
    XE_RS_FREE, 
    XE_RS_EMPTY,
    XE_RS_LOADING,
    XE_RS_STAGED,
    XE_RS_COMMITED,
    XE_RS_FAILED,
};

typedef struct xe_resource xe_resource;

struct xe_resource {
    uint16_t version;
    uint16_t state;
    uint32_t mask;
};

struct xe_res_image {
    struct xe_resource res;
    xe_rend_tex tex;
    const char *path;
    void *data;
    uint16_t w;
    uint16_t h;
    uint16_t c; /* channels */
    uint16_t flags;
};

xe_rend_tex xe_image_tex(xe_image image);
const struct xe_res_image *xe_image_ptr(xe_image image);

#endif /* XE_INTERNAL_SCENE_H */
