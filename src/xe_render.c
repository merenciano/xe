#include "xe_render.h"
#include "xe_render_internal.h"

#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <llulu/lu_hooks.h>
#include <llulu/lu_error.h>
#include <llulu/lu_log.h>
#include <glad/glad.h>

#include <string.h>
#include <stdint.h>

/*
    TODO:
        - Pipelines: Shader stages, render config and framebuffer. For skybox, post-process, shadowmaps, etc.
        - SPIR-V support
 */

enum {
    XE_MAX_VERTICES = 1U << 14,
    XE_MAX_INDICES = 1U << 14,
    XE_MAX_UNIFORMS = 256,
    XE_MAX_DRAW_INDIRECT = 256,
    XE_MAX_TEXTURE_ARRAYS = 16,
    XE_MAX_TEXTURE_LAYERS = 16,

    XE_MAX_SHADER_SOURCE_LEN = 4096,
    XE_MAX_ERROR_MSG_LEN = 2048,
    XE_MAX_SYNC_TIMEOUT_NANOSEC = 50000000
};

typedef struct xe_shader_frame_data {
    lu_mat4 view_proj;
    xe_shader_data data[XE_MAX_UNIFORMS];
} xe_shader_frame_data;

struct xe_texpool {
    xe_texfmt fmt[XE_MAX_TEXTURE_ARRAYS];
    int16_t layer_count[XE_MAX_TEXTURE_ARRAYS];
    uint32_t id[XE_MAX_TEXTURE_ARRAYS];
};

typedef struct xe_drawcmd {
    uint32_t element_count;
    uint32_t instance_count;
    uint32_t first_idx;
    int32_t  base_vtx;
    uint32_t draw_index; // base_instance
} xe_drawcmd;

enum {
    XE_VERTICES_MAP_SIZE = XE_MAX_VERTICES * 3 * sizeof(xe_vtx),
    XE_INDICES_MAP_SIZE = XE_MAX_INDICES * 3 * sizeof(xe_vtx_idx),
    XE_UNIFORMS_MAP_SIZE = 3 * sizeof(xe_shader_frame_data),
    XE_DRAW_INDIRECT_MAP_SIZE = XE_MAX_DRAW_INDIRECT * 3 * sizeof(xe_drawcmd),
};

/* Persistent mapped video buffer */
enum {
    XE_VBUF_MAP_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT,
    XE_VBUF_STORAGE_FLAGS = XE_VBUF_MAP_FLAGS | GL_DYNAMIC_STORAGE_BIT
};

typedef struct xe_vbuf {
    void *data;
    ptrdiff_t head;
    uint32_t id;
} xe_vbuf;

typedef struct xe_gl_renderer {
    struct xe_texpool tex;
    int phase; /* for the triphassic fence */
    GLsync fence[3]; // TODO typedef GLSync xe_gpu_fence
    uint32_t program_id;
    uint32_t vao_id;

    xe_vbuf vertices;
    xe_vbuf indices;
    xe_vbuf uniforms;
    xe_vbuf drawlist;

    xe_renderpass rpass;
    xe_draw_state curr_ops;
    lu_rect curr_vp;
    lu_color curr_bgcolor;
} xe_gl_renderer;

lu_mat4 view_projection;

static xe_gl_renderer g_r; // Depends on zero init

/* TODO move GL code to another file (backend) */
static const int g_tex_fmt_lut_internal[] = {
    GL_R8,
    GL_RG8,
    GL_RGB8,
    GL_SRGB8,
    GL_RGBA8,
    GL_R16F,
    GL_RG16F,
    GL_RGB16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
};

static const int g_tex_fmt_lut_format[] = {
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
};

static const int g_tex_fmt_lut_type[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
};

static const GLenum xe__lut_gl_blend[] = {
    0,
    0,
    GL_ONE,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ZERO
};

static const GLenum xe__lut_gl_cull[] = {
    0,
    0,
    GL_FRONT,
    GL_BACK,
    GL_FRONT_AND_BACK
};

static const GLenum xe__lut_gl_depth_fn[] = {
    0,
    0,
    GL_LEQUAL,
    GL_LESS
};

inline void
xe_render_sync(void)
{
    lu_timestamp start = lu_time_get();
    GLenum err = glClientWaitSync(g_r.fence[g_r.phase], GL_SYNC_FLUSH_COMMANDS_BIT, XE_MAX_SYNC_TIMEOUT_NANOSEC);
    int64_t sync_time_ns = lu_time_elapsed(start);
    if (err == GL_TIMEOUT_EXPIRED) {
        lu_log_err("Something is wrong with the gpu fences: sync blocked for more than %d ms.", (int)(XE_MAX_SYNC_TIMEOUT_NANOSEC / 1000000));
    } else if (err == GL_CONDITION_SATISFIED) {
        lu_log_warn("GPU fence blocked for %lld ns.", sync_time_ns);
    }
}

bool
xe_render_pipeline_compile(xe_program program, xe_shader_sources src)
{
    // TODO: Clean the code related to this src variables.
    const GLchar *vert_src1 = &src.vert_src[0];
    const GLchar *const *vert_src2 = &vert_src1;
    const GLchar *frag_src1 = &src.frag_src[0];
    const GLchar *const *frag_src2 = &frag_src1;

    GLchar out_log[XE_MAX_ERROR_MSG_LEN];
    GLint err;

    // Vert
    GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    GLint vert_length = (int)src.vert_len;
    glShaderSource(vert_id, 1, vert_src2, &vert_length);
    glCompileShader(vert_id);
    glGetShaderiv(vert_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(vert_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Vert Shader:\n%s\n", out_log);
        glDeleteShader(vert_id);
        return false;
    }
    glAttachShader(program, vert_id);

    // Frag
    GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);
    GLint frag_length = (int)src.frag_len;
    glShaderSource(frag_id, 1, frag_src2, &frag_length);
    glCompileShader(frag_id);
    glGetShaderiv(frag_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(frag_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Frag Shader:\n%s\n", out_log);
        glDetachShader(program, vert_id);
        glDeleteShader(frag_id);
        glDeleteShader(vert_id);
        return false;
    }
    glAttachShader(program, frag_id);

    // Program
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &err);
    if (!err) {
        glGetProgramInfoLog(program, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Program link error:\n%s\n", out_log);
        return false;
    }

    glDetachShader(program, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(program, frag_id);
    glDeleteShader(frag_id);
    if (!g_r.program_id) {
        g_r.program_id = program;
    }
    return true;
}

xe_program
xe_render_pipeline_alloc()
{
    return glCreateProgram();
}

xe_tex
xe_render_tex_alloc(xe_texfmt fmt)
{
    lu_err_assert(fmt.width + fmt.height != 0);
    lu_err_assert(fmt.format < XE_TEX_FMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < XE_MAX_TEXTURE_ARRAYS; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt)) && g_r.tex.layer_count[i] < XE_MAX_TEXTURE_LAYERS) {
            int layer = g_r.tex.layer_count[i]++;
            return (xe_tex){i, layer};
        }
    }

    // Initialize a new array for the texture
    for (int i = 0; i < XE_MAX_TEXTURE_ARRAYS; ++i) {
        if (!g_r.tex.layer_count[i]) {
            g_r.tex.fmt[i] = fmt;
            g_r.tex.layer_count[i] = 1;
            return (xe_tex){i, 0};
        }
    }

    lu_log_err("Error: xe_texture_reserve failed (full texture pool). Consider increasing XE_MAX_TEXTURE_ARRAYS.");
    return (xe_tex){-1, -1};
}

void
xe_render_tex_load(xe_tex tex, const void *data)
{
    lu_err_assert(tex.idx < XE_MAX_TEXTURE_ARRAYS && tex.layer < XE_MAX_TEXTURE_LAYERS);
    const xe_texfmt *fmt = &g_r.tex.fmt[tex.idx];
    lu_err_assert(fmt->width && fmt->height);
    GLint init;
    glGetTextureParameteriv(g_r.tex.id[tex.idx], GL_TEXTURE_IMMUTABLE_FORMAT, &init);
    if (init == GL_FALSE) {
        glTextureStorage3D(g_r.tex.id[tex.idx], 1, g_tex_fmt_lut_internal[fmt->format], fmt->width, fmt->height, XE_MAX_TEXTURE_LAYERS);
    }
    /* NULL data can be used to initialize the storage for writable textures */
    if (data) {
        glTextureSubImage3D(g_r.tex.id[tex.idx], 0, 0, 0, (int)tex.layer, fmt->width, fmt->height, 1, g_tex_fmt_lut_format[fmt->format], g_tex_fmt_lut_type[fmt->format], data);
    }
}

bool
xe_render_init(xe_renderconf *cfg)
{
    if (!cfg) {
        return false;
    }

    gladLoadGLLoader(cfg->gl_loader);

    for (int i = 0; i < 3; ++i) {
        g_r.fence[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    glViewport(cfg->viewport.x, cfg->viewport.y, cfg->viewport.w, cfg->viewport.h);
    g_r.curr_vp = cfg->viewport;
    glClearColor(0.0f, 0.0f, 0.4f, 1.0f);
    g_r.curr_bgcolor = (lu_color){0.0f, 0.0f, 0.4f, 1.0f};
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_WRITEMASK);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_STENCIL_WRITEMASK);
    g_r.curr_ops = (xe_draw_state){
        .clip = {0,0,0,0},
        .blend_src = XE_BLEND_ONE,
        .blend_dst = XE_BLEND_ONE_MINUS_SRC_ALPHA,
        .cull = XE_CULL_NONE,
        .depth = XE_DEPTH_DISABLED,
        .pipeline = XE_PROGRAM_UNSET
    };


    /* Persistent mapped buffers for vertices, indices uniforms and indirect draw commands. */
    GLuint buf_id[4];
    glCreateBuffers(4, buf_id);
    g_r.vertices.id = buf_id[0];
    glNamedBufferStorage(g_r.vertices.id, XE_VERTICES_MAP_SIZE, NULL, XE_VBUF_STORAGE_FLAGS);
    g_r.vertices.data = glMapNamedBufferRange(g_r.vertices.id, 0, XE_VERTICES_MAP_SIZE, XE_VBUF_MAP_FLAGS);
    g_r.indices.id = buf_id[1];
    glNamedBufferStorage(g_r.indices.id, XE_INDICES_MAP_SIZE, NULL, XE_VBUF_STORAGE_FLAGS);
    g_r.indices.data = glMapNamedBufferRange(g_r.indices.id, 0, XE_INDICES_MAP_SIZE, XE_VBUF_MAP_FLAGS);
    g_r.uniforms.id = buf_id[2];
    glNamedBufferStorage(g_r.uniforms.id, XE_UNIFORMS_MAP_SIZE, NULL, XE_VBUF_STORAGE_FLAGS);
    g_r.uniforms.data = glMapNamedBufferRange(g_r.uniforms.id, 0, XE_UNIFORMS_MAP_SIZE, XE_VBUF_MAP_FLAGS);
    g_r.uniforms.head = offsetof(xe_shader_frame_data, data);
    g_r.drawlist.id = buf_id[3];
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_r.drawlist.id);
    glNamedBufferStorage(g_r.drawlist.id, XE_DRAW_INDIRECT_MAP_SIZE, NULL, XE_VBUF_STORAGE_FLAGS);
    g_r.drawlist.data = glMapNamedBufferRange(g_r.drawlist.id, 0, XE_DRAW_INDIRECT_MAP_SIZE, XE_VBUF_MAP_FLAGS);

    /* Meshes */
    glCreateVertexArrays(1, &g_r.vao_id);
    GLuint id = g_r.vao_id;
    glBindVertexArray(id);
    glVertexArrayVertexBuffer(id, 0, g_r.vertices.id, 0, sizeof(xe_vtx));
    glVertexArrayElementBuffer(id, g_r.indices.id);

    glEnableVertexArrayAttrib(id, 0);
    glVertexArrayAttribFormat(id, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(id, 0, 0);

    glEnableVertexArrayAttrib(id, 1);
    glVertexArrayAttribFormat(id, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(id, 1, 0);

    glEnableVertexArrayAttrib(id, 2);
    glVertexArrayAttribFormat(id, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4 * sizeof(float));
    glVertexArrayAttribBinding(id, 2, 0);

    /* Textures  */
    glCreateTextures(GL_TEXTURE_2D_ARRAY, XE_MAX_TEXTURE_ARRAYS, g_r.tex.id);
    glBindTextures(0, XE_MAX_TEXTURE_ARRAYS, g_r.tex.id);

    float view[16];
    float proj[16];
    lu_mat4_look_at(view, (float[]){0.0f, 0.0f, 2.0f}, (float[]){0.0f, 0.0f, 0.0f}, (float[]){0.0f, 1.0f, 0.0f});
    lu_mat4_perspective_fov(proj, lu_radians(70.0f), cfg->viewport.w, cfg->viewport.h, 0.1f, 300.0f);
    lu_mat4_multiply(view_projection.m, proj, view);

    return true;
}

void
xe_render_pass_begin(lu_rect viewport, lu_color background,
                  bool clear_color, bool clear_depth, bool clear_stencil,
                  xe_draw_state ops)
{
    g_r.rpass.viewport = viewport;
    g_r.rpass.bg_color = background;
    g_r.rpass.clear_color = clear_color;
    g_r.rpass.clear_depth = clear_depth;
    g_r.rpass.clear_stencil = clear_stencil;
    g_r.rpass.head = 1;
    g_r.rpass.batches[0].start_offset = g_r.drawlist.head;
    g_r.rpass.batches[0].batch_size = 0;
    g_r.rpass.batches[0].state = ops;
    g_r.rpass.batches[1].start_offset = g_r.drawlist.head;
    g_r.rpass.batches[1].batch_size = 0;
    g_r.rpass.batches[1].state = ops;
    xe_render_sync();
}

void
xe_render_draw_state_set(xe_draw_state state)
{
    xe_draw_batch *curr = &g_r.rpass.batches[g_r.rpass.head];
    if (state.blend_src == XE_BLEND_UNSET || state.blend_dst == XE_BLEND_UNSET) {
        state.blend_src = curr->state.blend_src;
        state.blend_dst = curr->state.blend_dst;
    }

    if (state.depth == XE_DEPTH_UNSET) {
        state.depth = curr->state.depth;
    }

    if (state.cull == XE_CULL_UNSET) {
        state.cull = curr->state.cull;
    }

    if (state.pipeline == XE_PROGRAM_UNSET) {
        state.pipeline = curr->state.pipeline;
    }

    /* If current batch is empty or state change not needed: continue batch */
    if (curr->batch_size == 0 || (memcmp(&state, &curr->state, sizeof(state)) == 0)) {
        curr->state = state;
    } else {
        /* Add new batch */
        xe_draw_batch *new = &g_r.rpass.batches[++g_r.rpass.head];
        new->start_offset = g_r.drawlist.head;
        new->batch_size = 0;
        new->state = state;
    }
}

void
xe__vtxbuf_remaining(void **out_vtx, size_t *out_vtx_rem, size_t *out_first_vtx,
                     void **out_idx, size_t *out_idx_rem, size_t *out_first_idx)
{
#ifdef XE_DEBUG
    xe_render_sync();
#endif
    *out_vtx_rem = ((g_r.phase + 1) * XE_MAX_VERTICES * sizeof(xe_vtx)) - g_r.vertices.head;
    *out_idx_rem = ((g_r.phase + 1) * XE_MAX_INDICES * sizeof(xe_vtx_idx)) - g_r.indices.head;
    *out_vtx = (char*)g_r.vertices.data + g_r.vertices.head;
    *out_idx = (char*)g_r.indices.data + g_r.indices.head;
    *out_first_vtx = g_r.vertices.head / sizeof(xe_vtx);
    *out_first_idx = g_r.indices.head / sizeof(xe_vtx_idx);
}

void
xe__vtxbuf_push_nocheck(size_t vtx_bytes, size_t idx_bytes)
{
    g_r.vertices.head += vtx_bytes;
    g_r.indices.head += idx_bytes;
}

xe_mesh
xe_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size)
{
    void *vtx_head, *idx_head;
    size_t vtx_rem, idx_rem, base_vtx, base_idx;
    xe__vtxbuf_remaining(&vtx_head, &vtx_rem, &base_vtx, &idx_head, &idx_rem, &base_idx);
    lu_err_assert(vtx_head && idx_head);
    bool enough_space = (vert_size <= vtx_rem) && (indices_size <= idx_rem);
    lu_err_assert(enough_space);
    if (!enough_space) {
        return (xe_mesh){ .base_vtx = 0, .first_idx = 0, .idx_count = 0 };
    }

    xe_mesh added_mesh = { 
        .base_vtx = base_vtx,
        .first_idx = base_idx,
        .idx_count = (int)(indices_size / sizeof(xe_vtx_idx))
    };

    memcpy(vtx_head, vert, vert_size);
    memcpy(idx_head, indices, indices_size);
    xe__vtxbuf_push_nocheck(vert_size, indices_size);
    return added_mesh;
}

int
xe_material_add(const xe_material *mat)
{
#ifdef XE_DEBUG
    xe_render_sync();
#endif

    size_t remaining = ((g_r.phase + 1) * sizeof(xe_shader_frame_data)) - g_r.uniforms.head;
    bool enough_space = sizeof(xe_shader_data) <= remaining;
    lu_err_assert(enough_space);
    if (!enough_space) {
        return -1;
    }

    xe_shader_data *uniform = (void*)((char*)g_r.uniforms.data + g_r.uniforms.head);
    memcpy(uniform, &mat->data, sizeof(mat->data));
    size_t index = ((char*)(void*)uniform - ((char*)g_r.uniforms.data + g_r.phase * sizeof(xe_shader_frame_data) + offsetof(xe_shader_frame_data, data))) / sizeof(xe_shader_data);
    g_r.uniforms.head += sizeof(xe_shader_data);
    return (int)index;
}

bool
xe_drawcmd_add(xe_mesh mesh, int draw_id)
{
    size_t remaining = (g_r.phase + 1) * XE_MAX_DRAW_INDIRECT * sizeof(xe_drawcmd) - g_r.drawlist.head;
    bool enough_space = remaining >= sizeof(xe_drawcmd);
    lu_err_assert(enough_space);
    if (!enough_space) {
        return false;
    }

    *((xe_drawcmd*)((char*)g_r.drawlist.data + g_r.drawlist.head)) = (xe_drawcmd){
        .element_count = mesh.idx_count,
        .instance_count = 1,
        .first_idx = mesh.first_idx,
        .base_vtx = mesh.base_vtx,
        .draw_index = draw_id
    };
    g_r.drawlist.head += sizeof(xe_drawcmd);
    g_r.rpass.batches[g_r.rpass.head].batch_size++;
    return true;
}

void
xe_render_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, const xe_material *material)
{
    xe_mesh mesh = xe_mesh_add(vert, vert_size, indices, indices_size);
    int draw_id = xe_material_add(material);
    bool draw_cmd_ret = xe_drawcmd_add(mesh, draw_id);
    lu_err_assert(draw_cmd_ret && "Can not add draw command: draw indirect buffer full.");
}

void
xe_render_draw(void)
{
    lu_hook_notify(LU_HOOK_PRE_RENDER, &g_r);

    lu_err_assert(g_r.drawlist.head ==
            (g_r.rpass.batches[g_r.rpass.head].start_offset +
            g_r.rpass.batches[g_r.rpass.head].batch_size * sizeof(xe_drawcmd)));


    if (g_r.rpass.viewport.x != g_r.curr_vp.x ||
            g_r.rpass.viewport.y != g_r.curr_vp.y ||
            g_r.rpass.viewport.w != g_r.curr_vp.w ||
            g_r.rpass.viewport.h != g_r.curr_vp.h) {
        glViewport(g_r.rpass.viewport.x, g_r.rpass.viewport.y, g_r.rpass.viewport.w, g_r.rpass.viewport.h);
        g_r.curr_vp = g_r.rpass.viewport;
    }

    xe_program pipeline = g_r.rpass.batches[0].state.pipeline;
    if (!(pipeline == XE_PROGRAM_UNSET || (pipeline == g_r.curr_ops.pipeline))) {
        glUseProgram(pipeline);
        g_r.curr_ops.pipeline = pipeline;
    }

    if (memcmp(&g_r.rpass.batches[0].state.clip, &g_r.curr_ops.clip, sizeof(g_r.rpass.batches[0].state.clip)) != 0) {
        if ((g_r.rpass.batches[0].state.clip.x | g_r.rpass.batches[0].state.clip.y |
            g_r.rpass.batches[0].state.clip.w | g_r.rpass.batches[0].state.clip.h) == 0) {
            glDisable(GL_SCISSOR_TEST);
        } else {
            glEnable(GL_SCISSOR_TEST);
            glScissor(g_r.rpass.batches[0].state.clip.x, g_r.rpass.batches[0].state.clip.y, g_r.rpass.batches[0].state.clip.w, g_r.rpass.batches[0].state.clip.h);
        }

        g_r.curr_ops.clip = g_r.rpass.batches[0].state.clip;
    }

    if (!((g_r.rpass.batches[0].state.blend_src == XE_BLEND_UNSET || g_r.rpass.batches[0].state.blend_src == g_r.curr_ops.blend_src) &&
            (g_r.rpass.batches[0].state.blend_dst == XE_BLEND_UNSET || g_r.rpass.batches[0].state.blend_dst == g_r.curr_ops.blend_dst))) {
        if (g_r.rpass.batches[0].state.blend_src == XE_BLEND_DISABLED || g_r.rpass.batches[0].state.blend_dst == XE_BLEND_DISABLED) {
            glDisable(GL_BLEND);
            g_r.rpass.batches[0].state.blend_src = XE_BLEND_DISABLED;
            g_r.rpass.batches[0].state.blend_dst = XE_BLEND_DISABLED;
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(xe__lut_gl_blend[g_r.rpass.batches[0].state.blend_src],
                        xe__lut_gl_blend[g_r.rpass.batches[0].state.blend_dst]);
        }

        g_r.curr_ops.blend_src = g_r.rpass.batches[0].state.blend_src;
        g_r.curr_ops.blend_dst = g_r.rpass.batches[0].state.blend_dst;
    }

    if (!(g_r.rpass.batches[0].state.cull == XE_CULL_UNSET || g_r.rpass.batches[0].state.cull == g_r.curr_ops.cull)) {
        if (g_r.rpass.batches[0].state.cull == XE_CULL_NONE) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(xe__lut_gl_cull[g_r.rpass.batches[0].state.cull]);
        }

        g_r.curr_ops.cull = g_r.rpass.batches[0].state.cull;
    }

    if (!(g_r.rpass.batches[0].state.depth == XE_DEPTH_UNSET || g_r.rpass.batches[0].state.depth == g_r.curr_ops.depth)) {
        if (g_r.rpass.batches[0].state.depth == XE_DEPTH_DISABLED) {
            glDisable(GL_DEPTH_TEST);
        } else {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(xe__lut_gl_depth_fn[g_r.rpass.batches[0].state.depth]);
        }

        g_r.curr_ops.depth = g_r.rpass.batches[0].state.depth;
    }


    if (g_r.rpass.bg_color.r != g_r.curr_bgcolor.r ||
            g_r.rpass.bg_color.g != g_r.curr_bgcolor.g ||
            g_r.rpass.bg_color.b != g_r.curr_bgcolor.b ||
            g_r.rpass.bg_color.a != g_r.curr_bgcolor.a) {
        glClearColor(g_r.rpass.bg_color.r, g_r.rpass.bg_color.g, g_r.rpass.bg_color.b, g_r.rpass.bg_color.a);
        g_r.curr_bgcolor = g_r.rpass.bg_color;
    }

    glClear((g_r.rpass.clear_color   * GL_COLOR_BUFFER_BIT) |
            (g_r.rpass.clear_depth   * GL_DEPTH_BUFFER_BIT) |
            (g_r.rpass.clear_stencil * GL_STENCIL_BUFFER_BIT));

    xe_render_sync();
    memcpy((char*)g_r.uniforms.data + g_r.phase * sizeof(xe_shader_frame_data), view_projection.m, sizeof(view_projection));
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, g_r.uniforms.id, g_r.phase * sizeof(xe_shader_frame_data), sizeof(xe_shader_frame_data));

    xe_draw_state prev_ops = g_r.curr_ops;
    const int num_batches = g_r.rpass.head + 1;
    static const GLenum ELEM_TYPE = sizeof(xe_vtx_idx) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    for (int i = 0; i < num_batches; ++i) {
        xe_draw_batch *draw = &g_r.rpass.batches[i];

        if (!(draw->state.pipeline == XE_PROGRAM_UNSET || (draw->state.pipeline == g_r.curr_ops.pipeline))) {
            glUseProgram(draw->state.pipeline);
            g_r.curr_ops.pipeline = draw->state.pipeline;
        }

        if (memcmp(&draw->state.clip, &prev_ops.clip, sizeof(draw->state.clip)) != 0) {
            if ((draw->state.clip.x | draw->state.clip.y |
                draw->state.clip.w | draw->state.clip.h) == 0) {
                glDisable(GL_SCISSOR_TEST);
            } else {
                glEnable(GL_SCISSOR_TEST);
                glScissor((GLint)draw->state.clip.x, (GLint)draw->state.clip.y, (GLint)draw->state.clip.w, (GLint)draw->state.clip.h);
            }

            prev_ops.clip = draw->state.clip;
        }

        if (!((draw->state.blend_src == XE_BLEND_UNSET || draw->state.blend_src == prev_ops.blend_src) &&
              (draw->state.blend_dst == XE_BLEND_UNSET || draw->state.blend_dst == prev_ops.blend_dst))) {
            if (draw->state.blend_src == XE_BLEND_DISABLED || draw->state.blend_dst == XE_BLEND_DISABLED) {
                glDisable(GL_BLEND);
                draw->state.blend_src = XE_BLEND_DISABLED;
                draw->state.blend_dst = XE_BLEND_DISABLED;
            } else {
                glEnable(GL_BLEND);
                glBlendFunc(xe__lut_gl_blend[draw->state.blend_src],
                            xe__lut_gl_blend[draw->state.blend_dst]);
            }

            prev_ops.blend_src = draw->state.blend_src;
            prev_ops.blend_dst = draw->state.blend_dst;
        }

        if (!(draw->state.cull == XE_CULL_UNSET || draw->state.cull == prev_ops.cull)) {
            if (draw->state.cull == XE_CULL_NONE) {
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(xe__lut_gl_cull[draw->state.cull]);
            }

            prev_ops.cull = draw->state.cull;
        }

        if (!(draw->state.depth == XE_DEPTH_UNSET || draw->state.depth == prev_ops.depth)) {
            if (draw->state.depth == XE_DEPTH_DISABLED) {
                glDisable(GL_DEPTH_TEST);
            } else {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(xe__lut_gl_depth_fn[draw->state.depth]);
            }

            prev_ops.depth = draw->state.depth;
        }

        glMultiDrawElementsIndirect(
                GL_TRIANGLES, ELEM_TYPE,
                (void*)draw->start_offset,
                draw->batch_size, 0);

#if XE_VERBOSE
        lu_log_verbose("\nBatch %d:\ncmd count: %ld\n", i, draw->batch_size);
#endif
    }

    g_r.curr_ops = prev_ops;

#if XE_VERBOSE
    lu_log_verbose("\nTotal:\ncmd count: %ld\nvtx count: %ld\nidx count: %ld\n",
            g_r.drawlist.head / sizeof(xe_drawcmd),
            g_r.vertices.head / sizeof(xe_vtx),
            g_r.indices.head / sizeof(xe_vtx_idx));
#endif

    g_r.fence[g_r.phase] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_r.phase = (g_r.phase + 1) % 3;
    g_r.uniforms.head = g_r.phase * sizeof(xe_shader_frame_data) + offsetof(xe_shader_frame_data, data);
    g_r.drawlist.head = g_r.phase * XE_MAX_DRAW_INDIRECT * sizeof(xe_drawcmd);
    g_r.vertices.head = g_r.phase * XE_MAX_VERTICES * sizeof(xe_vtx);
    g_r.indices.head = g_r.phase * XE_MAX_INDICES * sizeof(xe_vtx_idx);

    lu_hook_notify(LU_HOOK_POST_RENDER, &g_r);
}

void
xe_render_shutdown(void)
{
    glFlush();
    glDeleteSync(g_r.fence[0]);
    glDeleteSync(g_r.fence[1]);
    glDeleteSync(g_r.fence[2]);
    glUnmapNamedBuffer(g_r.vertices.id);
    glUnmapNamedBuffer(g_r.indices.id);
    glUnmapNamedBuffer(g_r.uniforms.id);
    glUnmapNamedBuffer(g_r.drawlist.id);
    glDisableVertexAttribArray(g_r.vertices.id);
    glDeleteBuffers(1, &g_r.vertices.id);
    glDeleteBuffers(1, &g_r.indices.id);
    glDeleteBuffers(1, &g_r.uniforms.id);
    glDeleteBuffers(1, &g_r.drawlist.id);
    glDeleteTextures(XE_MAX_TEXTURE_ARRAYS, g_r.tex.id);
    glDeleteProgram(g_r.program_id);
    glDeleteVertexArrays(1, &g_r.vao_id);
}
