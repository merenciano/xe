#ifndef XE_CONFIG_H
#define XE_CONFIG_H

/* Internal buffer sizes */
enum {
    XE_SHADER_DATA_STRIDE = 256,
    XE_MAX_VERTICES = 1U << 14,
    XE_MAX_INDICES = 1U << 14,
    XE_MAX_DRAWS = 256, // Max draw commands per frame. Determines the size of draw_indirect and SSBO buffers.
    XE_MAX_TEXTURE_ARRAYS = 16, // New arrays are needed for storing different sizes and texfmts and when existing array has no layers left
    XE_MAX_TEXTURE_LAYERS = 16, // it's usually better to increase arrays instead of layers
    XE_MAX_PASSES = 16, // render_pass limit per frame. Should be greater than the amount of framebuffer changes needed per frame.

    XE_MAX_SHADER_SOURCE_LEN = 4096, // TODO: remove limits on glsl source length
    XE_MAX_ERROR_MSG_LEN = 2048, // TODO: remove limits. ask the app for a valid buf 
    XE_MAX_SYNC_TIMEOUT_NANOSEC = 50000000, // if waiting at gpu fences


    /* 
     * 3 for triple buffering
     * 2 for double buffering
     * I do no recommend any other values for real-time display rendering.
     */
    XE_GPU_MEMORY_BUFFERING = 3,
};

typedef unsigned short xe_vtx_idx; /* Type for vertex indices: uint16 or uint32 */

static const char *XE_DEFAULT_PBR_VERT_PATH = "./assets/vert.glsl";
static const char *XE_DEFAULT_PBR_FRAG_PATH = "./assets/frag.glsl";

#endif /* XE_CONFIG_H */
