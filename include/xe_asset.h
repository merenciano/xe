#ifndef XE_ASSET_H
#define XE_ASSET_H

#include <stdint.h>

typedef unsigned int xe_handle;

typedef struct {xe_handle id;} xe_image;
typedef struct {xe_handle id;} xe_pipeline;

enum xe_asset_state {
    XE_ASSET_FREE, 
    XE_ASSET_EMPTY,
    XE_ASSET_LOADING,
    XE_ASSET_STAGED,
    XE_ASSET_COMMITED,
    XE_ASSET_FAILED,
};

typedef struct xe_asset {
    uint16_t version;
    uint16_t state;
    const char *description;
} xe_asset;


enum {
    /* Compile-time config constants */
    XE_MAX_IMAGES = 32,
    XE_MAX_PIPELINES = 8,

    /* Flags */
    XE_IMG_PREMUL_ALPHA = 0x0001,
};

xe_image xe_image_load(const char *path, int tex_flags); // XE_IMG_ ... 
xe_image xe_image_load_data(const void *pix_data, int w, int h, int c, int tex_flags); // XE_IMG_ ...

xe_pipeline xe_asset_pipeline_load(const char *vert_path, const char *frag_path);


#endif /* XE_ASSET_H */
