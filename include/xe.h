#ifndef __XE_H__
#define __XE_H__

/*
    TODO:
     -> Utils: Alloc, Future, Async jobs, Hash map, Pool (sparse)
     -> Resource ref containerof (like kobject)
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

/*
 * SOURCE KEYWORDS (for grep)
 *  lu_ : utilities lib prefix (generic utilities collection)
 *  xe_ : project prefix (namespace)
 *  ent_ : entity prefix
 *  comp_ : component prefix
 *  sys_ : system prefix
 *  
 *  TODO: In comments. Future tasks, ideas or things to try.
 *  FIXME: In comments. Bugs that should be fixed anytime in the future (before the next minor version bump if possible)
 *  NOTE: In comments. Personal anotations
 *  API <module name>: In comments. Public api definitions.
 *
 *  handle, hnd: opaque reference to internally managed resource
 *  transform, tr: transformation matrix
 *  
 */


//  TODO: SpirV Shaders

#endif /* __XE_H__ */
