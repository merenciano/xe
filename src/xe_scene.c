#include "xe_scene.h"
#include "xe_platform.h"

#include <spine/spine.h>
#include <spine/extension.h>

void xe_scene_spine_update(xe_scene_spine *self, float delta_sec)
{
    spAnimationState_update(self->anim, delta_sec);
	spAnimationState_apply(self->anim, self->skel);
	spSkeleton_update(self->skel, delta_sec);
	spSkeleton_updateWorldTransform(self->skel, SP_PHYSICS_UPDATE);
}

void xe_scene_spine_draw(xe_scene_spine *self)
{
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    xe_rend_vtx vertbuf[2048];
    xe_rend_idx indibuf[2048];
    xe_rend_vtx *vertices = vertbuf;
    xe_rend_idx *indices = indibuf;
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
			spRegionAttachment_computeWorldVertices(region, slot, (float*)vertices, 0, sizeof(xe_rend_vtx) / sizeof(float));
            uv = region->uvs;
			self->node.tex = ((xe_rend_img *) (((spAtlasRegion *) region->rendererObject)->page->rendererObject))->tex;
		} else if (attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment *mesh = (spMeshAttachment *) attachment;
			attach_color = &mesh->color;

			// Early out if the slot color is 0
			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            slot_vtx_count = mesh->super.worldVerticesLength / 2;
			spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, slot_vtx_count * 2, (float*)vertices, 0, sizeof(xe_rend_vtx) / sizeof(float));
            uv = mesh->uvs;
            memcpy(indices, mesh->triangles, mesh->trianglesCount * sizeof(*indices));
            slot_idx_count = mesh->trianglesCount;
			self->node.tex = ((xe_rend_img *) (((spAtlasRegion *) mesh->rendererObject)->page->rendererObject))->tex;
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
            xe_rend_vtx *vtxit = vertices;
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

        xe_rend_mesh mesh = xe_rend_mesh_add(vertices, slot_vtx_count * sizeof(xe_rend_vtx), indices, slot_idx_count * sizeof(xe_rend_idx));
        xe_rend_draw_id di = xe_rend_material_add((xe_rend_material){
            .apx = self->node.pos_x, .apy = self->node.pos_y,
            .asx = self->node.scale_x, .asy = self->node.scale_y,
            .arot = self->node.rotation, .tex = self->node.tex
        });


        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
                xe_rend_submit(mesh, di);
                break;
            case SP_BLEND_MODE_MULTIPLY:
            	XE_LOG_VERBOSE("Multiply alpha blending not implemented.");
                xe_assert(0);
                break;
            case SP_BLEND_MODE_ADDITIVE:
            	XE_LOG_VERBOSE("Additive alpha blending not implemented.");
                xe_assert(0);
                break;
            case SP_BLEND_MODE_SCREEN:
            	XE_LOG_VERBOSE("Screen alpha blending not implemented.");
                xe_assert(0);
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
    xe_assert(self->atlas->rendererObject);
    self->rendererObject = self->atlas->rendererObject;
    XE_LOG_VERBOSE("Spine atlas: %s", ((xe_rend_img*)self->rendererObject)->path);
}

void _spAtlasPage_disposeTexture(spAtlasPage *self)
{
    (void)self;
}

char *_spUtil_readFile(const char *path, int *length)
{
    return _spReadFile(path, length);
}

#if 0
#define XE_SCENE_ITER_STACK_CAP 64

const xe_image_desc g_images[] = {
    { .id = {"owl"}, .img_path = "./assets/owl.png"},
    { .id = {"default"}, .img_path = "./assets/default_tex.png"},
    { .id = {"test"}, .img_path = "./assets/tex_test.png"},
};

const xe_shader_desc g_shaders[] = {
    { .id = {"generic"}, .vert_path = "./assets/vert.glsl", .frag_path = "./assets/frag.glsl" },
};

const xe_spine_desc g_spines[] = {
    { .id = {"owl"}, .atlas_path = "./assets/owl.atlas", .skeleton_path = "./assets/owl-pro.json" },
};

const xe_scenenode_desc g_nodes[] = {
    { 
        .pos_x = 0.0f, .pos_y = 0.0f, .scale_x = 1.0f, .scale_y = 1.0f, .rotation = 0.0f,
        .type = XE_NODE_SKELETON, .asset_id = {"owl"}, .child_count = 0
    }
};

enum {
    XE_ASSETS_IMG_COUNT = sizeof(g_images) / sizeof(g_images[0]),
    XE_ASSETS_SHADER_COUNT = sizeof(g_shaders) / sizeof(g_shaders[0]),
    XE_ASSETS_SPINE_COUNT = sizeof(g_spines) / sizeof(g_spines[0]),
    XE_SCENE_NODE_COUNT = sizeof(g_nodes) / sizeof(g_nodes[0])
};

typedef struct xe_scene_iter_state_t {
    int remaining_children;
    int parent_index;
    float abpx; /* absolute scale, position and rotation values */
    float abpy;
    float absx;
    float absy;
    float abrot;
} xe_scene_iter_state_t;

typedef struct xe_scene_iter_stack_t {
    xe_scene_iter_state_t buf[XE_SCENE_ITER_STACK_CAP];
    size_t count;
} xe_scene_iter_stack_t;

static inline const xe_scene_iter_state_t *
xe_scene_get_state(xe_scene_iter_stack_t *stack)
{
    static const xe_scene_iter_state_t EMPTY = { .remaining_children = 0, .parent_index = -1, .abpx = 0.0f, .abpy = 0.0f, .absx = 1.0f, .absy = 1.0f, .abrot = 0.0f };
    if (stack && stack->count > 0) {
        return &stack->buf[stack->count - 1];
    }
    return &EMPTY;
}

static inline bool
xe_scene_pop_state(xe_scene_iter_stack_t *stack)
{
    assert(stack);
    assert(stack->count > 0);
    return stack->count--;
}

static inline void
xe_scene_push_state(xe_scene_iter_stack_t *stack, int node_idx)
{
    const xe_scene_iter_state_t *curr = xe_scene_get_state(stack);
    const xe_scenenode_desc *node = &g_nodes[node_idx];
    stack->buf[stack->count].remaining_children = node->child_count;
    stack->buf[stack->count].parent_index = node_idx;
    stack->buf[stack->count].abrot = curr->abrot + node->rotation;
    stack->buf[stack->count].abpx = curr->abpx + node->pos_x;
    stack->buf[stack->count].abpy = curr->abpy + node->pos_y;
    stack->buf[stack->count].absx = curr->absx * node->scale_x;
    stack->buf[stack->count].absy = curr->absy * node->scale_y;
    stack->count++;
    assert(stack->count < XE_SCENE_ITER_STACK_CAP);
}

static inline bool
xe_scene_step_state(xe_scene_iter_stack_t *stack)
{
    return --stack->buf[stack->count - 1].remaining_children;
}

static inline void
xe_scene_update(void)
{
    xe_scene_iter_stack_t state = { .count = 0 };
    for (int i = 0; i < XE_SCENE_NODE_COUNT; ++i) {
        if (g_nodes[i].child_count > 0) {
            xe_scene_push_state(&state, i);

            if (g_nodes[i].type & (XE_NODE_SHAPE | XE_NODE_SKELETON)) {
                const xe_scene_iter_state_t *curr = xe_scene_get_state(&state);
                /*
                xe_gfx_process((xe_drawable_t){
                    .apx = curr->abpx,
                    .apy = curr->abpy,
                    .asx = curr->absx,
                    .asy = curr->absy,
                    .arot = curr->abrot,
                    .id = i
                });
                */
            }
            continue;
        }
        // TODO populate vertex buffer, uniform buffer and multidraw command buffer with the node if it has shape.

        // UPDATE NODE
        if (!xe_scene_step_state(&state)) {
            if (!xe_scene_pop_state(&state)) {
                assert((i + 1) == XE_SCENE_NODE_COUNT && "The scene graph only has one root node, so the current has to be the last one of the vector."); 
                break;
            }
        }
    }
}

#endif