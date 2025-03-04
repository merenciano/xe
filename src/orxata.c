#include "orxata.h"

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

#define ORX_VTXBUF_CAP (1024 * 1024)
#define ORX_IDXBUF_CAP (1024 * 1024)
#define ORX_TEXARR_CAP 16

// see: orx_shader_reload
#define ORX_SRCBUF_CAP 4096 
#define ORX_ERRMSG_CAP 2048
#define ORX_SHAPE_CAP 128

typedef struct orx_shader_shape_data_t {
    float pos_x;
    float pos_y;
    float scale_x;
    float scale_y;
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
    int idx_count;
    int vtx_count;
    orx_vertex_t vtxbuf[ORX_VTXBUF_CAP]; // TODO: better heap than static
    orx_index_t idxbuf[ORX_IDXBUF_CAP];

    orx_texarr_pool_t tex;
    int tex_count;

    orx_shader_data_t shader_data;

    orx_drawcmd_t drawlist[ORX_SHAPE_CAP];
    orx_drawcmd_t *draw_next;

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
    //glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
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
        glGetProgramInfoLog(g_r.program_id, ORX_ERRMSG_CAP, NULL, out_log);
        printf("Program link error:\n%s\n", out_log);
    }

    glUseProgram(g_r.program_id);
    glDetachShader(g_r.program_id, vert_id);
    glDeleteShader(vert_id);
    glDetachShader(g_r.program_id, frag_id);
    glDeleteShader(frag_id);
}

#ifndef WIN32
#define _stat stat
#else
#define fstat _stat
#endif

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

            int err = fstat(g_config.vert_shader_path, &stat);
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
                    err = fstat(g_config.frag_shader_path, &stat);
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
    glBindVertexArray(g_r.va_id);
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

    if (vtx_data) {
        memcpy((void*)(&g_r.vtxbuf[g_r.vtx_count]), vtx_data, vtx_count * sizeof(orx_vertex_t));
    }
    g_r.vtx_count += vtx_count; // Add count anyway since the buffer gap will be filled later.

    assert(g_r.vtx_count < ORX_VTXBUF_CAP);

    if (idx_data) {
        memcpy((void*)(&g_r.idxbuf[g_r.idx_count]), idx_data, idx_count * sizeof(orx_index_t));
    }
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

orx_image_t
orx_load_image(const char *path)
{
    orx_image_t img = {.path = path, .tex = {-1, -1} };
    img.data = stbi_load(img.path, &img.w, &img.h, &img.channels, 0);

    if (!img.data) {
        printf("Error loading the image %s.\nAborting execution.\n", img.path);
        exit(1);
    } else {
        img.tex = orx_texture_reserve((orx_texture_format_t){
            .width = img.w,
            .height = img.h,
            .pixel_fmt = img.channels == 4 ? ORX_PIXFMT_RGBA : ORX_PIXFMT_RGB,
            .flags = 0
        });
        assert(img.tex.idx >= 0);
    }
    return img;
}

void
orx_init(orx_config_t *cfg)
{
    g_r.draw_next = g_r.drawlist;

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
    //glBufferData(GL_DRAW_INDIRECT_BUFFER, (char*)g_r.draw_next - (char*)g_r.drawlist, g_r.drawlist, GL_DYNAMIC_DRAW);

}

void orx_draw_node(orx_node_t *node)
{
    // fill draw command buffer
    const int idx = g_r.draw_next - g_r.drawlist;
    g_r.draw_next->element_count = node->shape.idx_count;
    g_r.draw_next->instance_count = 1;
    g_r.draw_next->first_idx = node->shape.first_idx;
    g_r.draw_next->base_vtx = node->shape.base_vtx;
    g_r.draw_next->draw_index = idx;
    g_r.draw_next++;

    // fill shader storage buffer
    memcpy(&g_r.shader_data.shape[idx], node, 4 * sizeof(float));
    g_r.shader_data.shape[idx].tex_idx = (float)node->shape.texture.idx;
    g_r.shader_data.shape[idx].tex_layer = (float)node->shape.texture.layer;
}

void orx_render(void)
{
    // Check if any of the GLSL files has changed for hot-reload.
    // TODO: async
    orx_shader_check_reload();

    glClear(GL_COLOR_BUFFER_BIT);
    // sync mesh buffer
    // TODO: Persistent mapped fenced ring buffer ?? Spines at least
    glNamedBufferData(g_r.vb_id, g_r.vtx_count * sizeof(orx_vertex_t), (const void*)(&g_r.vtxbuf[0]), GL_STATIC_DRAW);
    glNamedBufferData(g_r.ib_id, g_r.idx_count * sizeof(orx_index_t), (const void*)(&g_r.idxbuf[0]), GL_STATIC_DRAW);
    // sync shader data buffer
    // TODO: Persistent mapped fenced ring buffer
    glBufferData(GL_UNIFORM_BUFFER, sizeof(orx_shader_data_t), &g_r.shader_data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, g_r.ub_id);
    // sync draw command buffer 
    // TODO: Persistent mapped fenced ring buffer
    glBufferData(GL_DRAW_INDIRECT_BUFFER, (char*)g_r.draw_next - (char*)g_r.drawlist, g_r.drawlist, GL_DYNAMIC_DRAW);
    glMultiDrawElementsIndirect(GL_TRIANGLES, sizeof(orx_index_t) == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0, g_r.draw_next - g_r.drawlist, 0);

    // reset cpu drawlist for the next frame
    g_r.draw_next = g_r.drawlist;
}

void orx_spine_update(orx_spine_t *self, float delta_sec)
{
    spAnimationState_update(self->anim, delta_sec);
	spAnimationState_apply(self->anim, self->skel);
	spSkeleton_update(self->skel, delta_sec);
	spSkeleton_updateWorldTransform(self->skel, SP_PHYSICS_UPDATE);
}

void orx_spine_draw(orx_spine_t *self)
{
    static orx_index_t quad_indices[] = {0, 1, 2, 2, 3, 0};
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    orx_vertex_t *vertices = &g_r.vtxbuf[self->node.shape.base_vtx];
    orx_index_t *indices = &g_r.idxbuf[self->node.shape.first_idx];
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

            assert((vertices - g_r.vtxbuf) <= g_r.vtx_count);
            assert((indices - g_r.idxbuf) <= self->node.shape.idx_count);

            memcpy(indices, quad_indices, sizeof(quad_indices));
            slot_idx_count = 6;
			slot_vtx_count = 4;
			spRegionAttachment_computeWorldVertices(region, slot, (float*)vertices, 0, sizeof(orx_vertex_t) / sizeof(float));
            uv = region->uvs;
			//self->node.shape.texture = ((orx_image_t *) (((spAtlasRegion *) region->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment *mesh = (spMeshAttachment *) attachment;
			attach_color = &mesh->color;

			// Early out if the slot color is 0
			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            assert((vertices - g_r.vtxbuf) <= g_r.vtx_count);
            assert((indices - g_r.idxbuf) <= self->node.shape.idx_count);

            slot_vtx_count = mesh->super.worldVerticesLength / 2;
            assert(slot_vtx_count <= g_r.vtx_count); // TODO: remove
			spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, slot_vtx_count * 2, (float*)vertices, 0, sizeof(orx_vertex_t) / sizeof(float));
            uv = mesh->uvs;
            memcpy(indices, mesh->triangles, mesh->trianglesCount * sizeof(*indices));
            slot_idx_count = mesh->trianglesCount;
			//self->node.shape.texture = ((orx_image_t *) (((spAtlasRegion *) mesh->rendererObject)->page->rendererObject))->tex;
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
            orx_vertex_t *vtxit = vertices;
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

        // fill draw command buffer
        const int idx = g_r.draw_next - g_r.drawlist;
        g_r.draw_next->element_count = slot_idx_count;
        g_r.draw_next->instance_count = 1;
        g_r.draw_next->first_idx = indices - g_r.idxbuf;
        g_r.draw_next->base_vtx = vertices - g_r.vtxbuf;
        g_r.draw_next->draw_index = idx;
        g_r.draw_next++;

        g_r.shader_data.shape[idx].pos_x = self->node.pos_x;
        g_r.shader_data.shape[idx].pos_y = self->node.pos_y;
        g_r.shader_data.shape[idx].scale_x = self->node.scale_x;
        g_r.shader_data.shape[idx].scale_y = self->node.scale_y;
        g_r.shader_data.shape[idx].tex_idx = self->node.shape.texture.idx;
        g_r.shader_data.shape[idx].tex_layer = (float)self->node.shape.texture.layer;

        vertices += slot_vtx_count;
        indices += slot_idx_count;

        /*
        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
                break;
            case SP_BLEND_MODE_MULTIPLY:
                break;
            case SP_BLEND_MODE_ADDITIVE:
                break;
            case SP_BLEND_MODE_SCREEN:
                break;
                */

		spSkeletonClipping_clipEnd(g_clipper, slot);
	}
	spSkeletonClipping_clipEnd2(g_clipper);
}

void _spAtlasPage_createTexture(spAtlasPage *self, const char *path)
{
    (void)path;
    assert(self->atlas->rendererObject);
    self->rendererObject = self->atlas->rendererObject;
#ifdef ORX_VERBOSE
    printf("Spine atlas: %s\n", ((orx_image_t*)self->rendererObject)->path);
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
