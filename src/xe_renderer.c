#include "xe_renderer.h"
#include "platform.h"

#include <llulu/lu_time.h>
#include <glad/glad.h>
#include <stb/stb_image.h>

#include <time.h>
#include <string.h>

enum {
    XE_MAX_VERTICES = 1U << 14,
    XE_MAX_INDICES = 1U << 14,
    XE_MAX_UNIFORMS = 128,
    XE_MAX_DRAW_INDIRECT = 128,
    XE_MAX_TEXTURE_ARRAYS = 16,
    XE_MAX_TEXTURE_ARRAY_SIZE = 16,

    XE_MAX_SHADER_SOURCE_LEN = 4096,
    XE_MAX_ERROR_MSG_LEN = 2048,
    XE_MAX_SYNC_TIMEOUT_NANOSEC = 50000000
};

typedef struct xe_shader_shape_data {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
    float rotation;
    int32_t tex_idx;
    float tex_layer;
    float padding;
} xe_shader_shape_data;

typedef struct xe_shader_data {
    float view_proj[16]; // Ortho projection matrix 
    xe_shader_shape_data data[XE_MAX_UNIFORMS];
} xe_shader_data;

typedef struct xe_texarr_pool {
    uint32_t id[XE_MAX_TEXTURE_ARRAYS];
    xe_rend_texfmt fmt[XE_MAX_TEXTURE_ARRAYS];
    int count[XE_MAX_TEXTURE_ARRAYS];
} xe_texarr_pool;

typedef struct xe_drawcmd {
    uint32_t element_count;
    uint32_t instance_count;
    uint32_t first_idx;
    int32_t  base_vtx;
    uint32_t draw_index; // base_instance
} xe_drawcmd;

/* Persistent mapped video buffer */
typedef struct xe_vbuf {
    enum {
        XE_GFX_MAP_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT,
        XE_GFX_STORAGE_FLAGS = XE_GFX_MAP_FLAGS | GL_DYNAMIC_STORAGE_BIT
    };
    void *data;
    size_t head;
    GLuint id;
} xe_vbuf;

typedef struct xe_gl_renderer {
    xe_texarr_pool tex;
    int tex_count;

    int phase; /* for the triphassic fence */
    GLsync fence[3];
    float view_proj[16]; // TODO: move to scene

    uint32_t program_id; // program opengl handle
    uint32_t vao_id; // vertex array object opengl handle
    xe_vbuf vertices;
    xe_vbuf indices;
    xe_vbuf uniforms;
    xe_vbuf drawlist;
} xe_gl_renderer;

enum {
    XE_VERTICES_MAP_SIZE = XE_MAX_VERTICES * 3 * sizeof(xe_rend_vtx),
    XE_INDICES_MAP_SIZE = XE_MAX_INDICES * 3 * sizeof(xe_rend_idx),
    XE_UNIFORMS_MAP_SIZE = 3 * sizeof(xe_shader_data),
    XE_DRAW_INDIRECT_MAP_SIZE = XE_MAX_DRAW_INDIRECT * 3 * sizeof(xe_drawcmd),
};

static xe_gl_renderer g_r; // Depends on zero init
static xe_rend_config g_config;

inline void
xe_rend_sync(void)
{
    lu_timestamp start = lu_time_get();
    GLenum err = glClientWaitSync(g_r.fence[g_r.phase], GL_SYNC_FLUSH_COMMANDS_BIT, XE_MAX_SYNC_TIMEOUT_NANOSEC);
    int64_t sync_time_ns = lu_time_elapsed(start);
    if (err == GL_TIMEOUT_EXPIRED) {
        printf("Error: something is wrong with the gpu fences: sync blocked for more than 5ms.\n");
    } else if (err == GL_CONDITION_SATISFIED) {
        printf("Warning: gpu fence blocked for %lld ns.\n", sync_time_ns);
    }
}

void
xe_shader_gpu_setup(void)
{
    // Ortho proj matrix
    float left = 0.0f;
    float right = (float)g_config.canvas.w;
    float bottom = (float)g_config.canvas.h;
    float top = 0.0f;
    float near = -1.0f;
    float far = 1.0f;
    g_r.view_proj[0] = 2.0f / (right - left);
    g_r.view_proj[5] = 2.0f / (top - bottom);
    g_r.view_proj[10] = -2.0f / (far - near);
    g_r.view_proj[12] = -(right + left) / (right - left);
    g_r.view_proj[13] = -(top + bottom) / (top - bottom);
    g_r.view_proj[14] = -(far + near) / (far - near);
    g_r.view_proj[15] = 1.0f;

    g_r.program_id = glCreateProgram();
    xe_rend_shader_reload();
}

void
xe_rend_shader_reload(void)
{
    char src_buf[XE_MAX_SHADER_SOURCE_LEN];
    const GLchar *src1 = &src_buf[0];
    const GLchar *const *src2 = &src1;
    // Vert
    FILE *f = fopen(g_config.vert_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.vert_shader_path);
        return;
    }
    size_t len = fread(&src_buf[0], 1, XE_MAX_SHADER_SOURCE_LEN- 1, f);
    src_buf[len] = '\0'; // TODO: pass len to glShaderSource instead of this (remove the '- 1' in the fread too).
    fclose(f);
    xe_assert(len < XE_MAX_SHADER_SOURCE_LEN);

    GLchar out_log[XE_MAX_ERROR_MSG_LEN];
    GLint err;
    GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_id, 1, src2, NULL);
    glCompileShader(vert_id);
    glGetShaderiv(vert_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(vert_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Vert Shader:\n%s\n", out_log);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.program_id, vert_id);

    // Frag
    f = fopen(g_config.frag_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.frag_shader_path);
        return;
    }
    len = fread(&src_buf[0], 1, XE_MAX_SHADER_SOURCE_LEN - 1, f);
    src_buf[len] = '\0';
    fclose(f);
    assert(len < XE_MAX_SHADER_SOURCE_LEN);

    GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_id, 1, src2, NULL); // see comment in the vertex shader
    glCompileShader(frag_id);
    glGetShaderiv(frag_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(frag_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Frag Shader:\n%s\n", out_log);
        glDetachShader(g_r.program_id, vert_id);
        glDeleteShader(frag_id);
        glDeleteShader(vert_id);
        return;
    }
    glAttachShader(g_r.program_id, frag_id);

    // Program
    glLinkProgram(g_r.program_id);
    glGetProgramiv(g_r.program_id, GL_LINK_STATUS, &err);
    if (!err) {
        glGetProgramInfoLog(g_r.program_id, XE_MAX_ERROR_MSG_LEN, NULL, out_log);
        printf("Program link error:\n%s\n", out_log);
    }

    glUseProgram(g_r.program_id);
    glDetachShader(g_r.program_id, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(g_r.program_id, frag_id);
    glDeleteShader(frag_id);
}


static void
xe_shader_check_reload(void)
{
    if (g_config.seconds_between_shader_file_changed_checks > 0.0f) {
        static int64_t timer;
        static int64_t last_modified;
        if (!timer) {
            assert(!last_modified);
            timer = time(NULL);
            last_modified = timer;
        }

        if (difftime(time(NULL), timer) > g_config.seconds_between_shader_file_changed_checks) {
            int64_t mtime = xe_file_mtime(g_config.vert_shader_path);
            if (mtime < 0) {
                XE_LOG_ERR("xe_file_mtime returned %lld", mtime);
            }
            bool reload = mtime > last_modified;
            if (!reload) {
                mtime = xe_file_mtime(g_config.frag_shader_path);
                if (mtime < 0) {
                    XE_LOG_ERR("xe_file_mtime returned %lld", mtime);
                }
                reload = mtime > last_modified;
            }

            if (reload) {
                last_modified = mtime;
                XE_LOG("Reloading shaders.");
                xe_rend_shader_reload();
            }
        }
        timer = time(NULL);
    }
}

void
xe_mesh_gpu_setup(void)
{
    glCreateVertexArrays(1, &g_r.vao_id);
    glBindVertexArray(g_r.vao_id);
    glVertexArrayVertexBuffer(g_r.vao_id, 0, g_r.vertices.id, 0, sizeof(xe_rend_vtx));
    glVertexArrayElementBuffer(g_r.vao_id, g_r.indices.id);

    glEnableVertexArrayAttrib(g_r.vao_id, 0);
    glVertexArrayAttribFormat(g_r.vao_id, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(g_r.vao_id, 0, 0);

    glEnableVertexArrayAttrib(g_r.vao_id, 1);
    glVertexArrayAttribFormat(g_r.vao_id, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(g_r.vao_id, 1, 0);

    glEnableVertexArrayAttrib(g_r.vao_id, 2);
    glVertexArrayAttribFormat(g_r.vao_id, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4 * sizeof(float));
    glVertexArrayAttribBinding(g_r.vao_id, 2, 0);
}

static const int g_tex_fmt_lut_internal[XE_TEX_FMT_COUNT] = {
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

static const int g_tex_fmt_lut_format[XE_TEX_FMT_COUNT] = {
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

static const int g_tex_fmt_lut_type[XE_TEX_FMT_COUNT] = {
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

static inline void
xe_texture_gpu_setup(void)
{
    glCreateTextures(GL_TEXTURE_2D_ARRAY, g_r.tex_count, g_r.tex.id);
    glBindTextures(0, g_r.tex_count, g_r.tex.id);
    for (int i = 0; i < g_r.tex_count; ++i) {
        const xe_rend_texfmt *fmt = &g_r.tex.fmt[i];
        glTextureStorage3D(g_r.tex.id[i], 1, g_tex_fmt_lut_internal[fmt->format], fmt->width, fmt->height, g_r.tex.count[i]);
    }
}

xe_rend_tex
xe_rend_tex_reserve(xe_rend_texfmt fmt)
{
    assert(fmt.width + fmt.height != 0);
    assert(fmt.format >= 0 && fmt.format < XE_TEX_FMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < g_r.tex_count; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt))) {
            int layer = g_r.tex.count[i]++;
            return (xe_rend_tex){i, layer};
        }
    }

    // If it is the first tex with that format, add a new vector to the pool.
    if (g_r.tex_count >= XE_MAX_TEXTURE_ARRAYS) {
        printf("Error: xe_texture_reserve failed (full texture pool). Consider increasing XE_MAX_TEXTURE_ARRAYS.\n");
        return (xe_rend_tex){-1, -1};
    }

    int index = g_r.tex_count++;
    g_r.tex.fmt[index] = fmt;
    g_r.tex.count[index] = 1;
    return (xe_rend_tex){index, 0};
}

void
xe_rend_tex_set(xe_rend_tex tex, void *data)
{
    assert(data);
    const xe_rend_texfmt *fmt = &g_r.tex.fmt[tex.idx];
    glTextureSubImage3D(g_r.tex.id[tex.idx], 0, 0, 0, (int)tex.layer, fmt->width, fmt->height, 1, g_tex_fmt_lut_format[fmt->format], g_tex_fmt_lut_type[fmt->format], data);
}

xe_rend_img
xe_rend_img_load(const char *path)
{
    xe_rend_img img = {.path = path, .tex = {-1, -1} };
    img.data = stbi_load(img.path, &img.w, &img.h, &img.channels, 0);

    if (!img.data) {
        printf("Error loading the image %s.\nAborting execution.\n", img.path);
        exit(1);
    } else {
        img.tex = xe_rend_tex_reserve((xe_rend_texfmt){
            .width = img.w,
            .height = img.h,
            .format = img.channels == 4 ? XE_TEX_RGBA : XE_TEX_RGB,
            .flags = 0
        });
        assert(img.tex.idx >= 0);
    }
    return img;
}

bool
xe_rend_init(xe_rend_config *cfg)
{
    if (!cfg) {
        return false;
    }

    g_config = *cfg;
    gladLoadGLLoader(cfg->gl_loader);

    for (int i = 0; i < 3; ++i) {
        g_r.fence[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(g_config.canvas.bg_col[0], g_config.canvas.bg_col[1], g_config.canvas.bg_col[2], 1.0f);
    glViewport(0, 0, g_config.canvas.w, g_config.canvas.h);

    /* Persistent mapped buffers for vertices, indices uniforms and indirect draw commands. */
    GLuint buf_id[4];
    glCreateBuffers(4, buf_id);
    g_r.vertices.id = buf_id[0];
    glNamedBufferStorage(g_r.vertices.id, XE_VERTICES_MAP_SIZE, NULL, XE_GFX_STORAGE_FLAGS);
    g_r.vertices.data = glMapNamedBufferRange(g_r.vertices.id, 0, XE_VERTICES_MAP_SIZE, XE_GFX_MAP_FLAGS);
    g_r.indices.id = buf_id[1];
    glNamedBufferStorage(g_r.indices.id, XE_INDICES_MAP_SIZE, NULL, XE_GFX_STORAGE_FLAGS);
    g_r.indices.data = glMapNamedBufferRange(g_r.indices.id, 0, XE_INDICES_MAP_SIZE, XE_GFX_MAP_FLAGS);
    g_r.uniforms.id = buf_id[2];
    glNamedBufferStorage(g_r.uniforms.id, XE_UNIFORMS_MAP_SIZE, NULL, XE_GFX_STORAGE_FLAGS);
    g_r.uniforms.data = glMapNamedBufferRange(g_r.uniforms.id, 0, XE_UNIFORMS_MAP_SIZE, XE_GFX_MAP_FLAGS);
    g_r.drawlist.id = buf_id[3];
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_r.drawlist.id);
    glNamedBufferStorage(g_r.drawlist.id, XE_DRAW_INDIRECT_MAP_SIZE, NULL, XE_GFX_STORAGE_FLAGS);
    g_r.drawlist.data = glMapNamedBufferRange(g_r.drawlist.id, 0, XE_DRAW_INDIRECT_MAP_SIZE, XE_GFX_MAP_FLAGS);

    xe_shader_gpu_setup();
    xe_mesh_gpu_setup();
    xe_texture_gpu_setup();
    return true;
}

xe_rend_mesh
xe_rend_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size)
{
#ifdef XE_DEBUG
    xe_rend_sync();
#endif
    assert(vert_size < (XE_VERTICES_MAP_SIZE / 3));
    assert(indices_size < (XE_INDICES_MAP_SIZE / 3));

    xe_rend_mesh mesh = { .idx_count = indices_size / sizeof(xe_rend_idx)};

    if ((g_r.vertices.head + vert_size) >= XE_VERTICES_MAP_SIZE) {
        g_r.vertices.head = 0;
    }
    mesh.base_vtx = g_r.vertices.head / sizeof(xe_rend_vtx);
    void *dst = (char*)g_r.vertices.data + g_r.vertices.head;
    memcpy(dst, vert, vert_size);
    g_r.vertices.head += vert_size;

    if ((g_r.indices.head + indices_size) >= XE_INDICES_MAP_SIZE) {
        g_r.indices.head = 0;
    }
    mesh.first_idx = g_r.indices.head / sizeof(xe_rend_idx);
    dst = (char*)g_r.indices.data + g_r.indices.head;
    memcpy(dst, indices, indices_size);
    g_r.indices.head += indices_size;
    return mesh;
}

xe_rend_draw_id
xe_rend_material_add(xe_rend_material mat)
{
#ifdef XE_DEBUG
    xe_rend_sync();
#endif
    assert(((g_r.uniforms.head - offsetof(xe_shader_data, data)) % sizeof(xe_shader_shape_data)) == 0);
    xe_shader_shape_data *uniform = (void*)((char*)g_r.uniforms.data + g_r.uniforms.head);
    uniform->pos_x = mat.apx;
    uniform->pos_y = mat.apy;
    uniform->scale_x = mat.asx;
    uniform->scale_y = mat.asy;
    uniform->rotation = mat.arot;
    uniform->tex_idx = mat.tex.idx;
    uniform->tex_layer = (float)mat.tex.layer;

    int idx = (g_r.uniforms.head - g_r.phase * sizeof(xe_shader_data) - offsetof(xe_shader_data, data)) / sizeof(xe_shader_shape_data);
    g_r.uniforms.head += sizeof(xe_shader_shape_data);
    return idx;
}

void
xe_rend_submit(xe_rend_mesh mesh, xe_rend_draw_id drawidx)
{
#ifdef XE_DEBUG
    xe_rend_sync();
#endif
    size_t frame_start = g_r.phase * (XE_DRAW_INDIRECT_MAP_SIZE / 3);
    assert(g_r.drawlist.head - frame_start < XE_DRAW_INDIRECT_MAP_SIZE / 3);

    *((xe_drawcmd*)(g_r.drawlist.data + g_r.drawlist.head)) = (xe_drawcmd){
        .element_count = mesh.idx_count,
        .instance_count = 1,
        .first_idx = mesh.first_idx,
        .base_vtx = mesh.base_vtx,
        .draw_index = drawidx 
    };
    g_r.drawlist.head += sizeof(xe_drawcmd);
}

void xe_rend_render(xe_rend_canvas *canvas)
{
    xe_shader_check_reload(); // TODO: async

    if (memcmp(&g_config.canvas, canvas, sizeof(g_config.canvas))) {
        g_config.canvas = *canvas;
        glClearColor(canvas->bg_col[0], canvas->bg_col[1], canvas->bg_col[2], 1.0f);
        glViewport(0, 0, canvas->w, canvas->h);
    }

    xe_rend_sync();
    memcpy(g_r.uniforms.data + g_r.phase * sizeof(xe_shader_data), g_r.view_proj, sizeof(g_r.view_proj));
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, g_r.uniforms.id, g_r.phase * sizeof(xe_shader_data), sizeof(xe_shader_data));

    glClear(
        ((int)g_config.canvas.clear_color * GL_COLOR_BUFFER_BIT) |
        ((int)g_config.canvas.clear_depth * GL_DEPTH_BUFFER_BIT) |
        ((int)g_config.canvas.clear_stencil * GL_STENCIL_BUFFER_BIT));

    size_t offset = (g_r.phase * (XE_DRAW_INDIRECT_MAP_SIZE / 3));
    size_t cmdbuf_size = g_r.drawlist.head - offset;
    assert(cmdbuf_size < (XE_DRAW_INDIRECT_MAP_SIZE / 3) && "The draw command list is larger than a third of the mapped buffer.");
    size_t cmdbuf_count = cmdbuf_size / sizeof(xe_drawcmd);
    assert(cmdbuf_count < XE_MAX_DRAW_INDIRECT);
    glMultiDrawElementsIndirect(GL_TRIANGLES, sizeof(xe_rend_idx) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)offset, cmdbuf_count, 0);

    g_r.fence[g_r.phase] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_r.phase = (g_r.phase + 1) % 3;
    g_r.uniforms.head = g_r.phase * sizeof(xe_shader_data) + offsetof(xe_shader_data, data);
    g_r.drawlist.head = g_r.phase * (XE_DRAW_INDIRECT_MAP_SIZE / 3);
}