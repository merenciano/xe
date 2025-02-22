#include "orxata.h"

#include <glad/glad.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define ORX_VERBOSE

#define ORX_VTXBUF_CAP (1024 * 1024)
#define ORX_IDXBUF_CAP (1024 * 1024)
#define ORX_TEXARR_CAP 16

// see: orx_shader_reload
#define ORX_SRCBUF_CAP 4096 
#define ORX_ERRMSG_CAP 2048
#define ORX_SHAPE_CAP 128

#define ORX_ARR_SIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))
#define ORX_IS_POW2(X) ((X > 0) && !((X) & ((X) - 1)))

typedef struct orx_shader_shape_data_t {
    float pos[2];
    float scale[2];
    int32_t tex_idx;
    float tex_layer;
    int32_t ipad[2];
} orx_shader_shape_data_t;

typedef struct orx_shader_data_t {
    float view_proj[16]; // Ortho projection matrix 
    orx_shader_shape_data_t shape[ORX_SHAPE_CAP];
} orx_shader_data_t;

typedef struct orx_texarr_pool_t {
    uint32_t id[ORX_TEXARR_CAP];
    orx_texture_format_t fmt[ORX_TEXARR_CAP];
    int count[ORX_TEXARR_CAP];
} orx_texarr_pool_t;

typedef struct orx_drawcmd {
    uint32_t element_count;
    uint32_t instance_count;
    uint32_t first_idx;
    int32_t  base_vtx;
    uint32_t draw_index; // base_instance
} orx_drawcmd_t;

typedef struct orx_gl_renderer_t {
    // TODO: better heap than static
    orx_vertex_t vtxbuf[ORX_VTXBUF_CAP];
    int vtx_count;

    orx_index_t idxbuf[ORX_IDXBUF_CAP];
    int idx_count;

    orx_texarr_pool_t tex;
    int tex_count;

    orx_shader_data_t shader_data;

    orx_drawcmd_t drawlist[ORX_SHAPE_CAP];
    int drawlist_count;

    uint32_t program_id;
    uint32_t va_id; // vertex array object 
    uint32_t vb_id; // vertex buffer
    uint32_t ib_id; // index buffer (elements)
    uint32_t ub_id; // uniform buffer
    uint32_t dib_id; // draw indirect command buffer
} orx_gl_renderer_t;

static orx_gl_renderer_t g_r; // Depends on zero init
static orx_config_t g_config;

void
orx_shader_gpu_setup(void)
{
    // Ortho proj matrix
    float left = 0.0f;
    float right = (float)g_config.canvas_width;
    float bottom = (float)g_config.canvas_height;
    float top = 0.0f;
    float near = -1.0f;
    float far = 1.0f;
    g_r.shader_data.view_proj[0] = 2.0f / (right - left);
    g_r.shader_data.view_proj[5] = 2.0f / (top - bottom);
    g_r.shader_data.view_proj[10] = -2.0f / (far - near);
    g_r.shader_data.view_proj[12] = -(right + left) / (right - left);
    g_r.shader_data.view_proj[13] = -(top + bottom) / (top - bottom);
    g_r.shader_data.view_proj[14] = -(far + near) / (far - near);
    g_r.shader_data.view_proj[15] = 1.0f;

    g_r.program_id = glCreateProgram();
    orx_shader_reload();
    glCreateBuffers(1, &g_r.ub_id);
    glBindBuffer(GL_UNIFORM_BUFFER, g_r.ub_id);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
}

void
orx_shader_reload(void)
{
    char src_buf[ORX_SRCBUF_CAP];
    const GLchar *src1 = &src_buf[0];
    const GLchar *const *src2 = &src1;
    // Vert
    FILE *f = fopen(g_config.vert_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.vert_shader_path);
        return;
    }
    size_t len = fread(&src_buf[0], 1, ORX_SRCBUF_CAP - 1, f);
    src_buf[len] = '\0'; // TODO: pass len to glShaderSource instead of this (remove the '- 1' in the fread too).
    fclose(f);
    assert(len < ORX_SRCBUF_CAP);

    GLchar out_log[ORX_ERRMSG_CAP];
    GLint err;
    GLuint vert_id = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_id, 1, src2, NULL);
    glCompileShader(vert_id);
    glGetShaderiv(vert_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(vert_id, ORX_ERRMSG_CAP, NULL, out_log);
        printf("Vert Shader:\n%s\n", out_log);
        return;
    }
    glAttachShader(g_r.program_id, vert_id);

    // Frag
    f = fopen(g_config.frag_shader_path, "r");
    if (!f) {
        printf("Could not open shader source %s.\n", g_config.frag_shader_path);
        return;
    }
    len = fread(&src_buf[0], 1, ORX_SRCBUF_CAP - 1, f);
    src_buf[len] = '\0';
    fclose(f);
    assert(len < ORX_SRCBUF_CAP);

    GLuint frag_id = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_id, 1, src2, NULL); // see comment in the vertex shader
    glCompileShader(frag_id);
    glGetShaderiv(frag_id, GL_COMPILE_STATUS, &err);
    if (!err) {
        glGetShaderInfoLog(frag_id, ORX_ERRMSG_CAP, NULL, out_log);
        printf("Frag Shader:\n%s\n", out_log);
        return;
    }
    glAttachShader(g_r.program_id, frag_id);

    // Program
    glLinkProgram(g_r.program_id);
    glGetProgramiv(g_r.program_id, GL_LINK_STATUS, &err);
    if (!err) {
        glGetProgramInfoLog(g_r.program_id, ORX_ERRMSG_CAP, NULL, out_log);
        printf("Program link error:\n%s\n", out_log);
    }

    glUseProgram(g_r.program_id);
    glDetachShader(g_r.program_id, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(g_r.program_id, frag_id);
    glDeleteShader(frag_id);
}

static void
orx_shader_check_reload(void)
{
    if (g_config.seconds_between_shader_file_changed_checks > 0.0f) {
        static time_t timer;
        static time_t last_modified;
        if (!timer) {
            assert(!last_modified);
            time(&timer);
            last_modified = timer;
        }

        if (difftime(time(NULL), timer) > g_config.seconds_between_shader_file_changed_checks) {
            struct _stat stat;
            int err = _stat(g_config.vert_shader_path, &stat);
            if (err) {
                if (err == -1) {
                    printf("%s not found.\n", g_config.vert_shader_path);
                } else if (err == 22) {
                    printf("Invalid parameter in _stat (vert).\n");
                }
            } else {
                if (difftime(stat.st_mtime, last_modified) > 0.0) {
                    last_modified = stat.st_mtime;
#ifdef ORX_VERBOSE
                    printf("Reloading shaders.\n");
#endif
                    orx_shader_reload();
                } else {
                    err = _stat(g_config.frag_shader_path, &stat);
                    if (err) {
                        if (err == -1) {
                            printf("%s not found.\n", g_config.frag_shader_path);
                        } else if (err == 22) {
                            printf("Invalid parameter in _stat (frag).\n");
                        }
                    } else {
                        if (difftime(stat.st_mtime, last_modified) > 0.0) {
                            last_modified = stat.st_mtime;
#ifdef ORX_VERBOSE
                            printf("Reloading shaders.\n");
#endif
                            orx_shader_reload();
                        }
                    }
                }
            }
            time(&timer);
        }
    }
}

void
orx_mesh_gpu_setup(void)
{
    glCreateBuffers(1, &g_r.vb_id);
    glCreateBuffers(1, &g_r.ib_id);
    glCreateVertexArrays(1, &g_r.va_id);
    glVertexArrayVertexBuffer(g_r.va_id, 0, g_r.vb_id, 0, sizeof(orx_vertex_t));
    glVertexArrayElementBuffer(g_r.va_id, g_r.ib_id);

    glEnableVertexArrayAttrib(g_r.va_id, 0);
    glVertexArrayAttribFormat(g_r.va_id, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(g_r.va_id, 0, 0);

    glEnableVertexArrayAttrib(g_r.va_id, 1);
    glVertexArrayAttribFormat(g_r.va_id, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(g_r.va_id, 1, 0);

    glEnableVertexArrayAttrib(g_r.va_id, 2);
    glVertexArrayAttribFormat(g_r.va_id, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4 * sizeof(float));
    glVertexArrayAttribBinding(g_r.va_id, 2, 0);
}

orx_shape_t
orx_mesh_add(const orx_vertex_t *vtx_data, int vtx_count, 
    const orx_index_t *idx_data, int idx_count)
{
    orx_shape_t shape;
    shape.base_vtx = g_r.vtx_count;
    shape.first_idx = g_r.idx_count;
    shape.idx_count = idx_count;
    shape.texture.idx = -1;
    //shape.texture.layer = -1.0f;
    shape.draw_index = -1;

    memcpy((void*)(&g_r.vtxbuf[g_r.vtx_count]), vtx_data, vtx_count * sizeof(orx_vertex_t));
    g_r.vtx_count += vtx_count;
    assert(g_r.vtx_count < ORX_VTXBUF_CAP);

    memcpy((void*)(&g_r.idxbuf[g_r.idx_count]), idx_data, idx_count * sizeof(orx_index_t));
    g_r.idx_count += idx_count;
    assert(g_r.idx_count < ORX_IDXBUF_CAP);

    return shape;
}

static const int g_tex_internfmt_lut[ORX_PIXFMT_COUNT] = {
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

static const int g_tex_format_lut[ORX_PIXFMT_COUNT] = {
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

static const int g_tex_typefmt_lut[ORX_PIXFMT_COUNT] = {
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
orx_texture_gpu_setup(void)
{
    glCreateTextures(GL_TEXTURE_2D_ARRAY, g_r.tex_count, g_r.tex.id);
    glBindTextures(0, g_r.tex_count, g_r.tex.id);
    for (int i = 0; i < g_r.tex_count; ++i) {
        const orx_texture_format_t *fmt = &g_r.tex.fmt[i];
        glTextureStorage3D(g_r.tex.id[i], 1, g_tex_internfmt_lut[fmt->pixel_fmt], fmt->width, fmt->height, g_r.tex.count[i]);
    }
}

orx_texture_t
orx_texture_reserve(orx_texture_format_t fmt)
{
    assert(fmt.width + fmt.height != 0);
    assert(fmt.pixel_fmt >= 0 && fmt.pixel_fmt < ORX_PIXFMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < g_r.tex_count; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt))) {
            int layer = g_r.tex.count[i]++;
            return (orx_texture_t){i, layer};
        }
    }

    // If it is the first tex with that format, add a new vector to the pool.
    if (g_r.tex_count >= ORX_TEXARR_CAP) {
        printf("Error: orx_texture_reserve failed (full texture pool). Consider increasing ORX_TEXARR_CAP.\n");
        return (orx_texture_t){-1, -1};
    }
    
    int index = g_r.tex_count++;
    g_r.tex.fmt[index] = fmt;
    g_r.tex.count[index] = 1;
    return (orx_texture_t){index, 0};
}

void
orx_texture_set(orx_texture_t tex, void *data)
{
    assert(data);
    const orx_texture_format_t *fmt = &g_r.tex.fmt[tex.idx];
    glTextureSubImage3D(g_r.tex.id[tex.idx], 0, 0, 0, (int)tex.layer, fmt->width, fmt->height, 1, g_tex_format_lut[fmt->pixel_fmt], g_tex_typefmt_lut[fmt->pixel_fmt], data);
}


void
orx_init(orx_config_t *cfg)
{
    if (cfg) {
        g_config = *cfg;
        gladLoadGLLoader(cfg->gl_loader);
    } else {
        g_config.canvas_width = 1024;
        g_config.canvas_height = 1024;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.2f, 0.0f, 0.2f, 1.0f);
    glViewport(0, 0, g_config.canvas_width, g_config.canvas_height);

    orx_shader_gpu_setup();
    orx_mesh_gpu_setup();
    orx_texture_gpu_setup();

    glGenBuffers(1, &g_r.dib_id);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, g_r.dib_id);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(orx_drawcmd_t) * g_r.drawlist_count, g_r.drawlist, GL_DYNAMIC_DRAW);

    glBindVertexArray(g_r.va_id);
}

void orx_draw_node(orx_node_t *node)
{
    orx_drawcmd_t *cmd = g_r.drawlist + g_r.drawlist_count;
    node->shape.draw_index = g_r.drawlist_count;
    cmd->instance_count = 1;
    cmd->draw_index = node->shape.draw_index;
    cmd->base_vtx = node->shape.base_vtx;
    cmd->first_idx = node->shape.first_idx;
    cmd->element_count = node->shape.idx_count;
    ++g_r.drawlist_count;

    orx_shader_shape_data_t *data = g_r.shader_data.shape + node->shape.draw_index;
    data->pos[0] = node->pos_x;
    data->pos[1] = node->pos_y;
    data->scale[0] = node->scale_x;
    data->scale[1] = node->scale_y;
    data->tex_idx = node->shape.texture.idx;
    data->tex_layer = (float)node->shape.texture.layer;
}

void orx_render(void)
{
    // Check if any of the GLSL files has changed for hot-reload.
    orx_shader_check_reload();

    glClear(GL_COLOR_BUFFER_BIT);

    // sync mesh buffer
    glNamedBufferData(g_r.vb_id, g_r.vtx_count * sizeof(orx_vertex_t), (const void*)(&g_r.vtxbuf[0]), GL_STATIC_DRAW);
    glNamedBufferData(g_r.ib_id, g_r.idx_count * sizeof(orx_index_t), (const void*)(&g_r.idxbuf[0]), GL_STATIC_DRAW);

    // sync shader data buffer
    glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_r.ub_id);

    // sync draw command buffer 
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(orx_drawcmd_t) * g_r.drawlist_count, g_r.drawlist, GL_DYNAMIC_DRAW);

    glMultiDrawElementsIndirect(GL_TRIANGLES, sizeof(orx_index_t) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0, g_r.drawlist_count, 0);

    // reset cpu list for the next frame
    g_r.drawlist_count = 0;
}
