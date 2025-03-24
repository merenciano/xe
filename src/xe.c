#include "xe.h"

#include <llulu/lu_time.h>
#include <spine/spine.h>
#include <spine/extension.h>
#include <glad/glad.h>
#include <stb/stb_image.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>
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
    xe_texture_format fmt[XE_MAX_TEXTURE_ARRAYS];
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
    XE_VERTICES_MAP_SIZE = XE_MAX_VERTICES * 3 * sizeof(xe_vertex),
    XE_INDICES_MAP_SIZE = XE_MAX_INDICES * 3 * sizeof(xe_index),
    XE_UNIFORMS_MAP_SIZE = 3 * sizeof(xe_shader_data),
    XE_DRAW_INDIRECT_MAP_SIZE = XE_MAX_DRAW_INDIRECT * 3 * sizeof(xe_drawcmd),
};

static xe_gl_renderer g_r; // Depends on zero init
static xe_config g_config;

static inline void
xe_gfx_sync(void)
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
    xe_shader_reload();
}

void
xe_shader_reload(void)
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
    assert(len < XE_MAX_SHADER_SOURCE_LEN);

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
    len = fread(&src_buf[0], 1, XE_MAX_SHADER_SOURCE_LEN- 1, f);
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


/* TODO: Move xe_shader_check_reload to platform module. And call it async  */
#ifndef _WIN32
#define XE_STAT_TYPE struct stat
#define XE_STAT_FUNC stat
#else
#define XE_STAT_TYPE struct _stat
#define XE_STAT_FUNC _stat
#endif

static void
xe_shader_check_reload(void)
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
            XE_STAT_TYPE st;

            int err = XE_STAT_FUNC(g_config.vert_shader_path, &st);
            if (err) {
                if (err == -1) {
                    printf("%s not found.\n", g_config.vert_shader_path);
                } else if (err == 22) {
                    printf("Invalid parameter in _stat (vert).\n");
                }
            } else {
                if (difftime(st.st_mtime, last_modified) > 0.0) {
                    last_modified = st.st_mtime;
#ifdef XE_VERBOSE
                    printf("Reloading shaders.\n");
#endif
                    xe_shader_reload();
                } else {
                    err = XE_STAT_FUNC(g_config.frag_shader_path, &st);
                    if (err) {
                        if (err == -1) {
                            printf("%s not found.\n", g_config.frag_shader_path);
                        } else if (err == 22) {
                            printf("Invalid parameter in _stat (frag).\n");
                        }
                    } else {
                        if (difftime(st.st_mtime, last_modified) > 0.0) {
                            last_modified = st.st_mtime;
#ifdef XE_VERBOSE
                            printf("Reloading shaders.\n");
#endif
                            xe_shader_reload();
                        }
                    }
                }
            }
            time(&timer);
        }
    }
}

void
xe_mesh_gpu_setup(void)
{
    glCreateVertexArrays(1, &g_r.vao_id);
    glBindVertexArray(g_r.vao_id);
    glVertexArrayVertexBuffer(g_r.vao_id, 0, g_r.vertices.id, 0, sizeof(xe_vertex));
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

static const int g_tex_fmt_lut_internal[XE_PIXFMT_COUNT] = {
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

static const int g_tex_fmt_lut_format[XE_PIXFMT_COUNT] = {
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

static const int g_tex_fmt_lut_type[XE_PIXFMT_COUNT] = {
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
        const xe_texture_format *fmt = &g_r.tex.fmt[i];
        glTextureStorage3D(g_r.tex.id[i], 1, g_tex_fmt_lut_internal[fmt->pixel_fmt], fmt->width, fmt->height, g_r.tex.count[i]);
    }
}

xe_texture
xe_texture_reserve(xe_texture_format fmt)
{
    assert(fmt.width + fmt.height != 0);
    assert(fmt.pixel_fmt >= 0 && fmt.pixel_fmt < XE_PIXFMT_COUNT && "Invalid pixel format.");

    // Look for an array of textures of the same format and push the new tex.
    for (int i = 0; i < g_r.tex_count; ++i) {
        if (!memcmp(&g_r.tex.fmt[i], &fmt, sizeof(fmt))) {
            int layer = g_r.tex.count[i]++;
            return (xe_texture){i, layer};
        }
    }

    // If it is the first tex with that format, add a new vector to the pool.
    if (g_r.tex_count >= XE_MAX_TEXTURE_ARRAYS) {
        printf("Error: xe_texture_reserve failed (full texture pool). Consider increasing XE_MAX_TEXTURE_ARRAYS.\n");
        return (xe_texture){-1, -1};
    }

    int index = g_r.tex_count++;
    g_r.tex.fmt[index] = fmt;
    g_r.tex.count[index] = 1;
    return (xe_texture){index, 0};
}

void
xe_texture_set(xe_texture tex, void *data)
{
    assert(data);
    const xe_texture_format *fmt = &g_r.tex.fmt[tex.idx];
    glTextureSubImage3D(g_r.tex.id[tex.idx], 0, 0, 0, (int)tex.layer, fmt->width, fmt->height, 1, g_tex_fmt_lut_format[fmt->pixel_fmt], g_tex_fmt_lut_type[fmt->pixel_fmt], data);
}

xe_image
xe_load_image(const char *path)
{
    xe_image img = {.path = path, .tex = {-1, -1} };
    img.data = stbi_load(img.path, &img.w, &img.h, &img.channels, 0);

    if (!img.data) {
        printf("Error loading the image %s.\nAborting execution.\n", img.path);
        exit(1);
    } else {
        img.tex = xe_texture_reserve((xe_texture_format){
            .width = img.w,
            .height = img.h,
            .pixel_fmt = img.channels == 4 ? XE_PIXFMT_RGBA : XE_PIXFMT_RGB,
            .flags = 0
        });
        assert(img.tex.idx >= 0);
    }
    return img;
}

bool
xe_init(xe_config *cfg)
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

xe_mesh
xe_gfx_add_mesh(const void *vert, size_t vert_size, const void *indices, size_t indices_size)
{
#ifdef XE_DEBUG
    xe_gfx_sync();
#endif
    assert(vert_size < (XE_VERTICES_MAP_SIZE / 3));
    assert(indices_size < (XE_INDICES_MAP_SIZE / 3));

    xe_mesh mesh = { .idx_count = indices_size / sizeof(xe_index)};

    if ((g_r.vertices.head + vert_size) >= XE_VERTICES_MAP_SIZE) {
        g_r.vertices.head = 0;
    }
    mesh.base_vtx = g_r.vertices.head / sizeof(xe_vertex);
    void *dst = (char*)g_r.vertices.data + g_r.vertices.head;
    memcpy(dst, vert, vert_size);
    g_r.vertices.head += vert_size;

    if ((g_r.indices.head + indices_size) >= XE_INDICES_MAP_SIZE) {
        g_r.indices.head = 0;
    }
    mesh.first_idx = g_r.indices.head / sizeof(xe_index);
    dst = (char*)g_r.indices.data + g_r.indices.head;
    memcpy(dst, indices, indices_size);
    g_r.indices.head += indices_size;
    return mesh;
}

xe_draw_idx
xe_gfx_add_material(xe_material mat)
{
#ifdef XE_DEBUG
    xe_gfx_sync();
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
xe_gfx_submit(xe_mesh mesh, xe_draw_idx drawidx)
{
#ifdef XE_DEBUG
    xe_gfx_sync();
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

void xe_render(xe_canvas *canvas)
{
    xe_shader_check_reload(); // TODO: async

    if (memcmp(&g_config.canvas, canvas, sizeof(g_config.canvas))) {
        g_config.canvas = *canvas;
        glClearColor(canvas->bg_col[0], canvas->bg_col[1], canvas->bg_col[2], 1.0f);
        glViewport(0, 0, canvas->w, canvas->h);
    }

    xe_gfx_sync();
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
    glMultiDrawElementsIndirect(GL_TRIANGLES, sizeof(xe_index) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)offset, cmdbuf_count, 0);

    g_r.fence[g_r.phase] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_r.phase = (g_r.phase + 1) % 3;
    g_r.uniforms.head = g_r.phase * sizeof(xe_shader_data) + offsetof(xe_shader_data, data);
    g_r.drawlist.head = g_r.phase * (XE_DRAW_INDIRECT_MAP_SIZE / 3);
}

void xe_spine_update(xe_spine *self, float delta_sec)
{
    spAnimationState_update(self->anim, delta_sec);
	spAnimationState_apply(self->anim, self->skel);
	spSkeleton_update(self->skel, delta_sec);
	spSkeleton_updateWorldTransform(self->skel, SP_PHYSICS_UPDATE);
}

void xe_spine_draw(xe_spine *self)
{
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    xe_vertex vertbuf[2048];
    xe_index indibuf[2048];
    xe_vertex *vertices = vertbuf;
    xe_index *indices = indibuf;
    int slot_idx_count = 0;
    int slot_vtx_count = 0;
    float *uv = NULL;
	spSkeleton *sk = self->skel;
	for (int i = 0; i < sk->slotsCount; ++i) {
		spSlot *slot = sk->drawOrder[i];
		spAttachment *attachment = slot->attachment;
		if (!attachment) {
			spSkeletonClipping_clipEnd(g_clipper, slot);
			continue;
		}

		if (slot->color.a == 0 || !slot->bone->active) {
			spSkeletonClipping_clipEnd(g_clipper, slot);
			continue;
		}

		spColor *attach_color = NULL;

		if (attachment->type == SP_ATTACHMENT_REGION) {
			spRegionAttachment *region = (spRegionAttachment *)attachment;
			attach_color = &region->color;

			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            indibuf[0] = 0;
            indibuf[1] = 1;
            indibuf[2] = 2;
            indibuf[3] = 2;
            indibuf[4] = 3;
            indibuf[5] = 0;
            slot_idx_count = 6;
			slot_vtx_count = 4;
			spRegionAttachment_computeWorldVertices(region, slot, (float*)vertices, 0, sizeof(xe_vertex) / sizeof(float));
            uv = region->uvs;
			self->node.tex = ((xe_image *) (((spAtlasRegion *) region->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment *mesh = (spMeshAttachment *) attachment;
			attach_color = &mesh->color;

			// Early out if the slot color is 0
			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            slot_vtx_count = mesh->super.worldVerticesLength / 2;
			spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, slot_vtx_count * 2, (float*)vertices, 0, sizeof(xe_vertex) / sizeof(float));
            uv = mesh->uvs;
            memcpy(indices, mesh->triangles, mesh->trianglesCount * sizeof(*indices));
            slot_idx_count = mesh->trianglesCount;
			self->node.tex = ((xe_image *) (((spAtlasRegion *) mesh->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_CLIPPING) {
			spClippingAttachment *clip = (spClippingAttachment *) slot->attachment;
			spSkeletonClipping_clipStart(g_clipper, slot, clip);
			continue;
		} else {
			continue;
        }

        uint32_t color = (uint8_t)(sk->color.r * slot->color.r * attach_color->r * 255);
        color |= ((uint8_t)(sk->color.g * slot->color.g * attach_color->g * 255) << 8);
        color |= ((uint8_t)(sk->color.b * slot->color.b * attach_color->b * 255) << 16);
        color |= ((uint8_t)(sk->color.a * slot->color.a * attach_color->a * 255) << 24);

        for (int v = 0; v < slot_vtx_count; ++v) {
            vertices[v].u = *uv++;
            vertices[v].v = *uv++;
            vertices[v].color = color;
        }

		if (spSkeletonClipping_isClipping(g_clipper)) {
            // TODO: Optimize but first try with spine-cpp-lite compiled as .so for C
            spSkeletonClipping_clipTriangles(g_clipper, (float*)vertices, slot_vtx_count * 2, indices, slot_idx_count, &vertices->u, sizeof(*vertices));
            slot_vtx_count = g_clipper->clippedVertices->size >> 1;
            xe_vertex *vtxit = vertices;
            float *xyit = g_clipper->clippedVertices->items;
            float *uvit = g_clipper->clippedUVs->items;
            for (int j = 0; j < slot_vtx_count; ++j) {
                vtxit->x = *xyit++;
                vtxit->u = *uvit++;
                vtxit->y = *xyit++;
                (vtxit++)->v = *uvit++;
            }
            slot_idx_count = g_clipper->clippedTriangles->size;
            memcpy(indices, g_clipper->clippedTriangles->items, slot_idx_count * sizeof(*indices));
		}

        xe_mesh shape = xe_gfx_add_mesh(vertices, slot_vtx_count * sizeof(xe_vertex), indices, slot_idx_count * sizeof(xe_index));
        xe_draw_idx di = xe_gfx_add_material((xe_material){
            .apx = self->node.pos_x, .apy = self->node.pos_y,
            .asx = self->node.scale_x, .asy = self->node.scale_y,
            .arot = self->node.rotation, .tex = self->node.tex
        });


        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
                xe_gfx_submit(shape, di);
                break;
            case SP_BLEND_MODE_MULTIPLY:
#ifdef XE_VERBOSE
            	printf("Multiply alpha blending not implemented.\n");
#endif /* XE_VERBOSE */
                assert(0);
                break;
            case SP_BLEND_MODE_ADDITIVE:
#ifdef XE_VERBOSE
            	printf("Additive alpha blending not implemented.\n");
#endif /* XE_VERBOSE */
                assert(0);
                break;
            case SP_BLEND_MODE_SCREEN:
#ifdef XE_VERBOSE
            	printf("Screen alpha blending not implemented.\n");
#endif /* XE_VERBOSE */
                assert(0);
                break;
        };

        vertices = vertbuf;
        indices = indibuf;
        slot_vtx_count = 0;
        slot_idx_count = 0;

		spSkeletonClipping_clipEnd(g_clipper, slot);
	}
	spSkeletonClipping_clipEnd2(g_clipper);
}

void _spAtlasPage_createTexture(spAtlasPage *self, const char *path)
{
    (void)path;
    assert(self->atlas->rendererObject);
    self->rendererObject = self->atlas->rendererObject;
#ifdef XE_VERBOSE
    printf("Spine atlas: %s\n", ((xe_image*)self->rendererObject)->path);
#endif
}

void _spAtlasPage_disposeTexture(spAtlasPage *self)
{
    (void)self;
}

char *_spUtil_readFile(const char *path, int *length)
{
    return _spReadFile(path, length);
}
