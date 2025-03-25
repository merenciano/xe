#ifndef __XE_H__
#define __XE_H__

/*
    TODO:
     -> Utils: Log, Alloc, Assert, Future, Async jobs, Hash map, Pool (sparse)
     -> Profiling/Debug: Tracing 
     -> Fill the scene with more assets
     -> UBO to SSAO
     -> Node hierarchy (transform matrix)
     -> Spine dark color (color + darkcolor in shader data?)
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
 * XE_CFG_LOG_VERBOSE: Print logs from verbose level of importance.
 *
 *
 * XE_CFG_ASSERT(COND): assert function. If not defined <assert.h> will be used.
 */

enum {
    XE_OK = 0,
    XE_ERR = -65, // XE_ERR and lower values are considered errors
    /* GENERIC DEFINITIONS (-64...-1, negative values to be able to use them as invalid indices or any other positive data) */
    XE_NOOP,
    XE_NULL,
    XE_DEFAULT,
    XE_DELETED,
    XE_INVALID,
    /* GENERIC ERRORS (-99...-66) */
    XE_ERR_FATAL = -99, // Unrecoverable error.
    XE_ERR_ARG,
    XE_ERR_CONFIG,
    XE_ERR_UNINIT, // Required module not initialized.
    XE_ERR_UNKNOWN,
    /* PLATFORM ERRORS (-199...-99) */
    XE_ERR_PLAT = -199,
    XE_ERR_PLAT_FILE,
    XE_ERR_PLAT_MEM,
    /* HANDLE ERRORS (-299...-200) */
    XE_ERR_HND = -299,
    XE_ERR_HND_DANGLING,
};

#endif /* __XE_H__ */
