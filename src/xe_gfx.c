#include "xe_gfx.h"
#include "xe_gfx_internal.h"
#include "xe_gfx_helpers.h"
#include "xe_platform.h"

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

typedef struct xe_shader_shape_data {
    lu_mat4 model;
    lu_vec4 color; // TODO: Remove since it's in the vertex already
    lu_vec4 darkcolor; // darkColor.a == PMA
    int32_t albedo_idx;
    float albedo_layer;
    float pma; // TODO: Use DarkColor.a ???
    float padding;
} xe_shader_shape_data;

typedef struct xe_shader_data {
    lu_mat4 view_proj;
    xe_shader_shape_data data[XE_MAX_UNIFORMS];
} xe_shader_data;

struct xe_texpool {
    xe_gfx_texfmt fmt[XE_MAX_TEXTURE_ARRAYS];
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
    XE_VERTICES_MAP_SIZE = XE_MAX_VERTICES * 3 * sizeof(xe_gfx_vtx),
    XE_INDICES_MAP_SIZE = XE_MAX_INDICES * 3 * sizeof(xe_gfx_idx),
    XE_UNIFORMS_MAP_SIZE = 3 * sizeof(xe_shader_data),
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

typedef struct xe_shader {
    const char *vert_path;
    const char *frag_path;
    uint32_t program_id;
    uint32_t vao_id;
} xe_shader;

typedef struct xe_pipeline {
    xe_shader shader;
} xe_pipeline;

typedef struct xe_gl_renderer {
    struct xe_texpool tex;
    int phase; /* for the triphassic fence */
    GLsync fence[3];
    lu_mat4 view_proj; // TODO: move to scene
    xe_pipeline pipeline;

    xe_vbuf vertices;
    xe_vbuf indices;
    xe_vbuf uniforms;
    xe_vbuf drawlist;

    xe_gfx_renderpass rpass;
    xe_gfx_rops curr_ops;
    lu_rect curr_vp;
    lu_color curr_bgcolor;
} xe_gl_renderer;


static xe_gl_renderer g_r; // Depends on zero init

inline void
xe_gfx_sync(void)
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

static void
xe_shader_load_src(const char *vert_src, size_t vert_len, const char *frag_src, size_t frag_len)
{
    // TODO: Clean the code related to this src variables.
    const GLchar *vert_src1 = &vert_src[0];
    const GLchar *const *vert_src2 = &vert_src1;
    const GLchar *frag_src1 = &frag_src[0];
    const GLchar *const *frag_src2 = &frag_src1;

    GLchar out_log[XE_MAX_ERROR_MSG_LEN];
    GLint err;

    // Vert
    GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    GLint vert_length = (int)vert_len;
    glShaderSource(vert_id, 1, vert_src2, &vert_length);
    glCompileShader(vert_id);
    glGetShaderiv(vert_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(vert_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Vert Shader:\n%s\n", out_log);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.pipeline.shader.program_id, vert_id);

    // Frag
    GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);
    GLint frag_length = (int)frag_len;
    glShaderSource(frag_id, 1, frag_src2, &frag_length); // see comment in the vertex shader
    glCompileShader(frag_id);
    glGetShaderiv(frag_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(frag_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Frag Shader:\n%s\n", out_log);
        glDetachShader(g_r.pipeline.shader.program_id, vert_id);
        glDeleteShader(frag_id);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.pipeline.shader.program_id, frag_id);

    // Program
    glLinkProgram(g_r.pipeline.shader.program_id);
    glGetProgramiv(g_r.pipeline.shader.program_id, GL_LINK_STATUS, &err);
    if (!err) {
        glGetProgramInfoLog(g_r.pipeline.shader.program_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        lu_log_err("Program link error:\n%s\n", out_log);
    }

    glUseProgram(g_r.pipeline.shader.program_id);
    glDetachShader(g_r.pipeline.shader.program_id, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(g_r.pipeline.shader.program_id, frag_id);
    glDeleteShader(frag_id);
}

// TODO: Move this function to asset management. This translation unit should not interact with OS files.
static void
xe_shader_load_path(const char *vert_path, const char *frag_path)
{
    char vert_buf[XE_MAX_SHADER_SOURCE_LEN];
    char frag_buf[XE_MAX_SHADER_SOURCE_LEN];

    // Vert
    size_t vert_len;
    bool ret = xe_file_read(vert_path, vert_buf, XE_MAX_SHADER_SOURCE_LEN - 1, &vert_len);
    lu_err_assert(ret);
    vert_buf[vert_len] = '\0';  // TODO: pass len to glShaderSource instead of this (remove the '- 1' in the fread too).


    // Frag
    size_t frag_len;
    ret = xe_file_read(frag_path, frag_buf, XE_MAX_SHADER_SOURCE_LEN - 1, &frag_len);
    lu_err_assert(ret);
    frag_buf[frag_len] = '\0';  // TODO: pass len to glShaderSource instead of this (remove the '- 1' in the fread too).

    xe_shader_load_src(vert_buf, vert_len, frag_buf, frag_len);
}

static void
xe_shader_gpu_setup(void)
{
    float view[16];
    float proj[16];
    lu_mat4_look_at(view, (float[]){0.0f, 0.0f, 2.0f}, (float[]){0.0f, 0.0f, 0.0f}, (float[]){0.0f, 1.0f, 0.0f});
    lu_mat4_perspective_fov(proj, lu_radians(70.0f), 1920.0f, 1080.0f, 0.1f, 300.0f);
    lu_mat4_multiply(g_r.view_proj.m, proj, view);

    g_r.pipeline.shader.program_id = glCreateProgram();
    xe_shader_load_path(g_r.pipeline.shader.vert_path, g_r.pipeline.shader.frag_path);
}

xe_gfx_tex
xe_gfx_tex_alloc(xe_gfx_texfmt fmt)
{
    lu_err_assert(fmt.width + fmt.height != 0);
    lu_err_assert(fmt.format < XE_TEX_FMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < XE_MAX_TEXTURE_ARRAYS; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt)) && g_r.tex.layer_count[i] < XE_MAX_TEXTURE_LAYERS) {
            int layer = g_r.tex.layer_count[i]++;
            return (xe_gfx_tex){i, layer};
        }
    }

    // Initialize a new array for the texture
    for (int i = 0; i < XE_MAX_TEXTURE_ARRAYS; ++i) {
        if (!g_r.tex.layer_count[i]) {
            g_r.tex.fmt[i] = fmt;
            g_r.tex.layer_count[i] = 1;
            return (xe_gfx_tex){i, 0};
        }
    }

    lu_log_err("Error: xe_texture_reserve failed (full texture pool). Consider increasing XE_MAX_TEXTURE_ARRAYS.");
    return (xe_gfx_tex){-1, -1};
}

void
xe_gfx_tex_load(xe_gfx_tex tex, void *data)
{
    lu_err_assert(tex.idx < XE_MAX_TEXTURE_ARRAYS && tex.layer < XE_MAX_TEXTURE_LAYERS);
    const xe_gfx_texfmt *fmt = &g_r.tex.fmt[tex.idx];
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
xe_gfx_init(xe_gfx_config *cfg)
{
    if (!cfg) {
        return false;
    }

    gladLoadGLLoader(cfg->gl_loader);

    for (int i = 0; i < 3; ++i) {
        g_r.fence[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    g_r.pipeline.shader.vert_path = cfg->vert_shader_path;
    g_r.pipeline.shader.frag_path = cfg->frag_shader_path;

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
    g_r.curr_ops = (xe_gfx_rops){
        .clip = {0,0,0,0},
        .blend_src = XE_BLEND_ONE,
        .blend_dst = XE_BLEND_ONE_MINUS_SRC_ALPHA,
        .cull = XE_CULL_NONE,
        .depth = XE_DEPTH_DISABLED
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
    g_r.uniforms.head = offsetof(xe_shader_data, data);
    g_r.drawlist.id = buf_id[3];
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_r.drawlist.id);
    glNamedBufferStorage(g_r.drawlist.id, XE_DRAW_INDIRECT_MAP_SIZE, NULL, XE_VBUF_STORAGE_FLAGS);
    g_r.drawlist.data = glMapNamedBufferRange(g_r.drawlist.id, 0, XE_DRAW_INDIRECT_MAP_SIZE, XE_VBUF_MAP_FLAGS);

    xe_shader_gpu_setup();

    /* Meshes */
    glCreateVertexArrays(1, &g_r.pipeline.shader.vao_id);
    GLuint id = g_r.pipeline.shader.vao_id;
    glBindVertexArray(id);
    glVertexArrayVertexBuffer(id, 0, g_r.vertices.id, 0, sizeof(xe_gfx_vtx));
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

    return true;
}

void
xe_gfx_pass_begin(lu_rect viewport, lu_color background,
                  bool clear_color, bool clear_depth, bool clear_stencil,
                  xe_gfx_rops ops)
{
    g_r.rpass.viewport = viewport;
    g_r.rpass.bg_color = background;
    g_r.rpass.clear_color = clear_color;
    g_r.rpass.clear_depth = clear_depth;
    g_r.rpass.clear_stencil = clear_stencil;
    g_r.rpass.head = 0;
    g_r.rpass.batches[0].start_offset = g_r.drawlist.head;
    g_r.rpass.batches[0].batch_size = 0;
    g_r.rpass.batches[0].rops = ops;
}

void
xe_gfx_rops_set(xe_gfx_rops ops)
{
    xe_gfx_draw_batch *curr = &g_r.rpass.batches[g_r.rpass.head];
    if (ops.blend_src == XE_BLEND_UNSET || ops.blend_dst == XE_BLEND_UNSET) {
        ops.blend_src = curr->rops.blend_src;
        ops.blend_dst = curr->rops.blend_dst;
    }

    if (ops.depth == XE_DEPTH_UNSET) {
        ops.depth = curr->rops.depth;
    }

    if (ops.cull == XE_CULL_UNSET) {
        ops.cull = curr->rops.cull;
    }

    /* If current batch is empty or state change not needed: continue batch */
    if (curr->batch_size == 0 || (memcmp(&ops, &curr->rops, sizeof(ops)) == 0)) {
        curr->rops = ops;
    } else {
        /* Add new batch */
        xe_gfx_draw_batch *new = &g_r.rpass.batches[++g_r.rpass.head];
        new->start_offset = g_r.drawlist.head;
        new->batch_size = 0;
        new->rops = ops;
    }
}

xe_mesh
xe_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size)
{
#ifdef XE_DEBUG
    xe_gfx_sync();
#endif
    size_t vert_remaining = ((g_r.phase + 1) * XE_MAX_VERTICES * sizeof(xe_gfx_vtx)) - g_r.vertices.head;
    size_t indices_remaining = ((g_r.phase + 1) * XE_MAX_INDICES * sizeof(xe_gfx_idx)) - g_r.indices.head;
    bool enough_space = (vert_size <= vert_remaining) && (indices_size <= indices_remaining);
    lu_err_assert(enough_space);
    if (!enough_space) {
        return (xe_mesh){ .base_vtx = 0, .first_idx = 0, .idx_count = 0 };
    }

    xe_mesh added_mesh = { 
        .base_vtx = (int)(g_r.vertices.head / sizeof(xe_gfx_vtx)),
        .first_idx = (int)(g_r.indices.head / sizeof(xe_gfx_idx)),
        .idx_count = (int)(indices_size / sizeof(xe_gfx_idx))
    };

    memcpy((char*)g_r.vertices.data + g_r.vertices.head, vert, vert_size);
    g_r.vertices.head += vert_size;
    memcpy((char*)g_r.indices.data + g_r.indices.head, indices, indices_size);
    g_r.indices.head += indices_size;
    
    return added_mesh;
}

int
xe_material_add(const xe_gfx_material *mat)
{
#ifdef XE_DEBUG
    xe_gfx_sync();
#endif

    size_t remaining = ((g_r.phase + 1) * sizeof(xe_shader_data)) - g_r.uniforms.head;
    bool enough_space = sizeof(xe_shader_shape_data) <= remaining;
    lu_err_assert(enough_space);
    if (!enough_space) {
        return -1;
    }

    //int idx = ((ptrdiff_t)g_r.uniforms.head - g_r.phase * sizeof(xe_shader_data) - offsetof(xe_shader_data, data))
    //            / sizeof(xe_shader_shape_data);

    //int index = g_r.uniforms.head - ((((char*)g_r.uniforms.data) + g_r.phase * sizeof(xe_shader_data)) + offsetof(xe_shader_data, data));

    xe_shader_shape_data *uniform = (void*)((char*)g_r.uniforms.data + g_r.uniforms.head);
    uniform->model = mat->model;
    uniform->color = mat->color;
    uniform->darkcolor = mat->darkcolor;
    uniform->albedo_idx = mat->tex.idx;
    uniform->albedo_layer = (float)mat->tex.layer;
    uniform->pma = mat->pma;

    size_t index = ((char*)(void*)uniform - ((char*)g_r.uniforms.data + g_r.phase * sizeof(xe_shader_data) + offsetof(xe_shader_data, data))) / sizeof(xe_shader_shape_data);
    g_r.uniforms.head += sizeof(xe_shader_shape_data);
    
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
xe_gfx_push(const void *vert, size_t vert_size, const void *indices, size_t indices_size, const xe_gfx_material *material)
{
    xe_mesh mesh = xe_mesh_add(vert, vert_size, indices, indices_size);
    int draw_id = xe_material_add(material);
    bool draw_cmd_ret = xe_drawcmd_add(mesh, draw_id);
    lu_err_assert(draw_cmd_ret && "Can not add draw command: draw indirect buffer full.");
}

void
xe_gfx_render(void)
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

    if (memcmp(&g_r.rpass.batches[0].rops.clip, &g_r.curr_ops.clip, sizeof(g_r.rpass.batches[0].rops.clip)) != 0) {
        if ((g_r.rpass.batches[0].rops.clip.x | g_r.rpass.batches[0].rops.clip.y |
            g_r.rpass.batches[0].rops.clip.w | g_r.rpass.batches[0].rops.clip.h) == 0) {
            glDisable(GL_SCISSOR_TEST);
        } else {
            glEnable(GL_SCISSOR_TEST);
            glScissor(g_r.rpass.batches[0].rops.clip.x, g_r.rpass.batches[0].rops.clip.y, g_r.rpass.batches[0].rops.clip.w, g_r.rpass.batches[0].rops.clip.h);
        }

        g_r.curr_ops.clip = g_r.rpass.batches[0].rops.clip;
    }

    if (!((g_r.rpass.batches[0].rops.blend_src == XE_BLEND_UNSET || g_r.rpass.batches[0].rops.blend_src == g_r.curr_ops.blend_src) &&
            (g_r.rpass.batches[0].rops.blend_dst == XE_BLEND_UNSET || g_r.rpass.batches[0].rops.blend_dst == g_r.curr_ops.blend_dst))) {
        if (g_r.rpass.batches[0].rops.blend_src == XE_BLEND_DISABLED || g_r.rpass.batches[0].rops.blend_dst == XE_BLEND_DISABLED) {
            glDisable(GL_BLEND);
            g_r.rpass.batches[0].rops.blend_src = XE_BLEND_DISABLED;
            g_r.rpass.batches[0].rops.blend_dst = XE_BLEND_DISABLED;
        } else {
            glEnable(GL_BLEND);
            glBlendFunc(xe__lut_gl_blend[g_r.rpass.batches[0].rops.blend_src],
                        xe__lut_gl_blend[g_r.rpass.batches[0].rops.blend_dst]);
        }

        g_r.curr_ops.blend_src = g_r.rpass.batches[0].rops.blend_src;
        g_r.curr_ops.blend_dst = g_r.rpass.batches[0].rops.blend_dst;
    }

    if (!(g_r.rpass.batches[0].rops.cull == XE_CULL_UNSET || g_r.rpass.batches[0].rops.cull == g_r.curr_ops.cull)) {
        if (g_r.rpass.batches[0].rops.cull == XE_CULL_NONE) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(xe__lut_gl_cull[g_r.rpass.batches[0].rops.cull]);
        }

        g_r.curr_ops.cull = g_r.rpass.batches[0].rops.cull;
    }

    if (!(g_r.rpass.batches[0].rops.depth == XE_DEPTH_UNSET || g_r.rpass.batches[0].rops.depth == g_r.curr_ops.depth)) {
        if (g_r.rpass.batches[0].rops.depth == XE_DEPTH_DISABLED) {
            glDisable(GL_DEPTH_TEST);
        } else {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(xe__lut_gl_depth_fn[g_r.rpass.batches[0].rops.depth]);
        }

        g_r.curr_ops.depth = g_r.rpass.batches[0].rops.depth;
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

    xe_gfx_sync();
    memcpy((char*)g_r.uniforms.data + g_r.phase * sizeof(xe_shader_data), g_r.view_proj.m, sizeof(g_r.view_proj));
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, g_r.uniforms.id, g_r.phase * sizeof(xe_shader_data), sizeof(xe_shader_data));

    xe_gfx_rops prev_ops = g_r.curr_ops;
    const int num_batches = g_r.rpass.head + 1;
    static const GLenum ELEM_TYPE = sizeof(xe_gfx_idx) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
    for (int i = 0; i < num_batches; ++i) {
        xe_gfx_draw_batch *draw = &g_r.rpass.batches[i];

        if (memcmp(&draw->rops.clip, &prev_ops.clip, sizeof(draw->rops.clip)) != 0) {
            if ((draw->rops.clip.x | draw->rops.clip.y |
                draw->rops.clip.w | draw->rops.clip.h) == 0) {
                glDisable(GL_SCISSOR_TEST);
            } else {
                glEnable(GL_SCISSOR_TEST);
                glScissor(draw->rops.clip.x, draw->rops.clip.y, draw->rops.clip.w, draw->rops.clip.h);
            }

            prev_ops.clip = draw->rops.clip;
        }

        if (!((draw->rops.blend_src == XE_BLEND_UNSET || draw->rops.blend_src == prev_ops.blend_src) &&
              (draw->rops.blend_dst == XE_BLEND_UNSET || draw->rops.blend_dst == prev_ops.blend_dst))) {
            if (draw->rops.blend_src == XE_BLEND_DISABLED || draw->rops.blend_dst == XE_BLEND_DISABLED) {
                glDisable(GL_BLEND);
                draw->rops.blend_src = XE_BLEND_DISABLED;
                draw->rops.blend_dst = XE_BLEND_DISABLED;
            } else {
                glEnable(GL_BLEND);
                glBlendFunc(xe__lut_gl_blend[draw->rops.blend_src],
                            xe__lut_gl_blend[draw->rops.blend_dst]);
            }

            prev_ops.blend_src = draw->rops.blend_src;
            prev_ops.blend_dst = draw->rops.blend_dst;
        }

        if (!(draw->rops.cull == XE_CULL_UNSET || draw->rops.cull == prev_ops.cull)) {
            if (draw->rops.cull == XE_CULL_NONE) {
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(xe__lut_gl_cull[draw->rops.cull]);
            }

            prev_ops.cull = draw->rops.cull;
        }

        if (!(draw->rops.depth == XE_DEPTH_UNSET || draw->rops.depth == prev_ops.depth)) {
            if (draw->rops.depth == XE_DEPTH_DISABLED) {
                glDisable(GL_DEPTH_TEST);
            } else {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(xe__lut_gl_depth_fn[draw->rops.depth]);
            }

            prev_ops.depth = draw->rops.depth;
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
            (g_r.drawlist.head / sizeof(xe_drawcmd)) % XE_MAX_DRAW_INDIRECT,
            (g_r.vertices.head / sizeof(xe_gfx_vtx)) % XE_MAX_VERTICES,
            (g_r.indices.head / sizeof(xe_gfx_idx)) % XE_MAX_INDICES);
#endif

    g_r.fence[g_r.phase] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_r.phase = (g_r.phase + 1) % 3;
    g_r.uniforms.head = g_r.phase * sizeof(xe_shader_data) + offsetof(xe_shader_data, data);
    g_r.drawlist.head = g_r.phase * XE_MAX_DRAW_INDIRECT * sizeof(xe_drawcmd);
    g_r.vertices.head = g_r.phase * XE_MAX_VERTICES * sizeof(xe_gfx_vtx);
    g_r.indices.head = g_r.phase * XE_MAX_INDICES * sizeof(xe_gfx_idx);

    lu_hook_notify(LU_HOOK_POST_RENDER, &g_r);
}

void
xe_gfx_shutdown(void)
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
    glDeleteProgram(g_r.pipeline.shader.program_id);
    glDeleteVertexArrays(1, &g_r.pipeline.shader.vao_id);
}

