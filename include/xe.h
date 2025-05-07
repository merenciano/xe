#ifndef __XE_H__
#define __XE_H__

/*
    TODO:
     -> Utils: Alloc, Future, Async jobs, Hash map, Pool (sparse)
     -> Profiling/Debug: Tracing 
     -> Fill the scene with more assets
     -> UBO to SSAO
     -> Node hierarchy (transform matrix)
     -> Blend modes (and depth, stencil, scissor, clip, viewport, clear, cull face)
     -> Text rendering
     -> Nuklear or cimgui integration

    Modules:
    - App
    - Platform
    - Scene (glTF 2.0)
        - glTF 2.0 import / export
        - Asset manager
        - Node graph: High lvl api (handles)
        - ECS: handle to internal representation and detailed API (lock-free async sched based on W/R of system updates)
    - Renderer OpenGL 4.6
*/


/*
 * CONFIG COMPILE OPTIONS
 * 
 * XE_CFG_LOG_DISABLED: Disable log printing.
 * XE_CFG_LOG_RESTRICTED: Only print error logs.
 *
 * XE_CFG_ASSERT(COND): assert function. If not defined <assert.h> will be used.
 */

/* Image API */
enum {
    XE_MAX_IMAGES = 32,
};

enum {
    XE_IMG_PREMUL_ALPHA = 0x0001,
};

typedef unsigned int xe_handle;
typedef struct {xe_handle id;} xe_image;

xe_image xe_image_load(const char *path, int tex_flags); // XE_IMG_ ... 

//  TODO: SpirV Shaders

#endif /* __XE_H__ */
