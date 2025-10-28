#include "xe_spine.h"
#include "xe_gfx.h"
#include "../src/xe_scene_internal.h"

#include <xe_platform.h>

#include <llulu/lu_defs.h>
#include <llulu/lu_error.h>
#include <llulu/lu_log.h>
#include <spine/spine.h>
#include <spine/extension.h>

enum {
    XE_SP_FILENAME_LEN = 256,
    XE_MAX_SPINES = 32
};

struct xe_res_spine {
    struct xe_resource res;
    xe_scene_node node;
    spSkeleton *skel;
    spAnimationState *anim;
};

struct xe_atlas_entry {
    char filename[XE_SP_FILENAME_LEN];
    struct xe_atlas_entry *next; 
    spAtlas *atlas;
    spSkeletonJson *skel_json;
    spSkeletonData *skel_data;
    spAnimationStateData *anim_data;
    xe_image img;
};

struct xe_atlas_entry *g_atlases = NULL;

static struct xe_res_spine g_spines[XE_MAX_SPINES];

void
xe_spine_animate(struct xe_res_spine *self, float delta_sec)
{
    spAnimationState_update(self->anim, delta_sec);
	spAnimationState_apply(self->anim, self->skel);
	spSkeleton_update(self->skel, delta_sec);
	spSkeleton_updateWorldTransform(self->skel, SP_PHYSICS_UPDATE);
}

void
xe_spine_animation_pass(float delta_time)
{
    for (int i = 0; i < XE_MAX_SPINES; ++i) {
        if (g_spines[i].res.state == XE_RS_COMMITED) {
            xe_spine_animate(g_spines + i, delta_time);
        }
    }
}

void *
xe_spine_get_skel(xe_scene_node node)
{
    return g_spines[xe_res_index(node.hnd)].skel;
}

void *
xe_spine_get_anim(xe_scene_node node)
{
    return g_spines[xe_res_index(node.hnd)].anim;
}

/*
 * Vertex capacity of the spine batches. The batches always break between skeletons and on every material variable
 * change (e.g. dark_color or blend additive blend because of the zeroed alpha), this is because a new index is needed
 * to read from another material instance, so it's the simplest thing to do. Anyway it's the best thing to do since
 * it's not 'really' batching since we are grouping indirect draw commands, not drawcalls. The major problem was
 * the growth of the draw indirect buffer on scenes with a lot of spines with little slots.
 * TODO: Heap instead of static.
 */
enum { XE_SPBATCH_VTX_CAP = 1024 << 5, XE_SPBATCH_IDX_CAP = 1024 << 6 };
struct slot_batch {
    xe_gfx_vtx vert[XE_SPBATCH_VTX_CAP];
    xe_gfx_idx indices[XE_SPBATCH_IDX_CAP << 1];
    int64_t vtx_count;
    int64_t idx_count;
    xe_gfx_material material;
};

/* TODO: Go back to colored vertices but keep dark color with the materials, so Additive blend can zero its alpha
 * without using another draw indirect command for the index. */
int
xe_spine_draw(lu_mat4 *tr, void *draw_ctx)
{
    static struct slot_batch current_batch;
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    lu_err_assert(tr && draw_ctx);

    current_batch.material.model = *tr;
    current_batch.material.darkcolor = LU_VEC(0.0f, 0.0f, 0.0f, 1.0f);
    current_batch.vtx_count = 0;
    current_batch.idx_count = 0;

    xe_gfx_vtx vertbuf[2048];
    xe_gfx_idx indibuf[2048];
    xe_gfx_vtx *vertices = vertbuf;
    xe_gfx_idx *indices = indibuf;
    int slot_idx_count = 0;
    int slot_vtx_count = 0;
    float *uv = NULL;
	spSkeleton *sk = ((struct xe_res_spine*)draw_ctx)->skel;
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
			spRegionAttachment_computeWorldVertices(region, slot, (float*)vertices, 0, sizeof(xe_gfx_vtx) / sizeof(float));
            uv = region->uvs;
			const struct xe_res_image *pimg = xe_image_ptr(*((xe_image*)((spAtlasRegion *)region->rendererObject)->page->rendererObject));
            current_batch.material.pma = pimg->flags & XE_IMG_PREMUL_ALPHA;
            current_batch.material.tex = pimg->tex;
		} else if (attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment *mesh = (spMeshAttachment *) attachment;
			attach_color = &mesh->color;

			// Early out if the slot color is 0
			if (attach_color->a == 0) {
				spSkeletonClipping_clipEnd(g_clipper, slot);
				continue;
			}

            slot_vtx_count = mesh->super.worldVerticesLength / 2;
			spVertexAttachment_computeWorldVertices(SUPER(mesh), slot, 0, slot_vtx_count * 2, (float*)vertices, 0, sizeof(xe_gfx_vtx) / sizeof(float));
            uv = mesh->uvs;
            memcpy(indices, mesh->triangles, mesh->trianglesCount * sizeof(*indices));
            slot_idx_count = mesh->trianglesCount;
			const struct xe_res_image *pimg = xe_image_ptr(*((xe_image*)((spAtlasRegion *)mesh->rendererObject)->page->rendererObject));
            current_batch.material.pma = pimg->flags & XE_IMG_PREMUL_ALPHA;
            current_batch.material.tex = pimg->tex;
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
        if (slot->data->blendMode != SP_BLEND_MODE_ADDITIVE) {
            color |= ((uint8_t)(sk->color.a * slot->color.a * attach_color->a * 255) << 24);
        }

        for (int v = 0; v < slot_vtx_count; ++v) {
            vertices[v].u = *uv++;
            vertices[v].v = *uv++;
            vertices[v].color = color;
        }

		if (spSkeletonClipping_isClipping(g_clipper)) {
            // TODO: Optimize but first try with spine-cpp-lite compiled as .so for C
            spSkeletonClipping_clipTriangles(g_clipper, (float*)vertices, slot_vtx_count * 2, indices, slot_idx_count, &vertices->u, sizeof(*vertices));
            slot_vtx_count = g_clipper->clippedVertices->size >> 1;
            xe_gfx_vtx *vtxit = vertices;
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

        lu_vec4 dark_color = slot->darkColor ? LU_VEC(slot->darkColor->r, slot->darkColor->g, slot->darkColor->b, 1.0f) : LU_VEC(0.0f, 0.0f, 0.0f, 1.0f);

        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
            case SP_BLEND_MODE_ADDITIVE:
                break;

            case SP_BLEND_MODE_MULTIPLY:
            	lu_log_verbose("Multiply alpha blending not implemented.");
                lu_err_assert(0);
                break;
            case SP_BLEND_MODE_SCREEN:
            	lu_log_verbose("Screen alpha blending not implemented.");
                lu_err_assert(0);
                break;
        };

        if ((current_batch.vtx_count << 1 > XE_SPBATCH_VTX_CAP) || (current_batch.idx_count << 1 > XE_SPBATCH_IDX_CAP) ||
                (current_batch.vtx_count && (memcmp(&current_batch.material.darkcolor, &dark_color, sizeof(dark_color))))) {
            xe_gfx_push(current_batch.vert, current_batch.vtx_count * sizeof(xe_gfx_vtx),
                current_batch.indices, current_batch.idx_count * sizeof(xe_gfx_idx), &current_batch.material);
            current_batch.idx_count = 0;
            current_batch.vtx_count = 0;
        }

        current_batch.material.darkcolor = dark_color;
        memcpy(&current_batch.vert[current_batch.vtx_count], vertices, slot_vtx_count * sizeof(xe_gfx_vtx));
        for (int i = 0; i < slot_idx_count; ++i) {
            current_batch.indices[current_batch.idx_count + i] = indices[i] + current_batch.vtx_count;
        }
        current_batch.vtx_count += slot_vtx_count;
        current_batch.idx_count += slot_idx_count;
        vertices = vertbuf;
        indices = indibuf;
        slot_vtx_count = 0;
        slot_idx_count = 0;

		spSkeletonClipping_clipEnd(g_clipper, slot);
	}
	spSkeletonClipping_clipEnd2(g_clipper);

    if (current_batch.vtx_count) {
        xe_gfx_push(current_batch.vert, current_batch.vtx_count * sizeof(xe_gfx_vtx),
            current_batch.indices, current_batch.idx_count * sizeof(xe_gfx_idx), &current_batch.material);
        current_batch.idx_count = 0;
        current_batch.vtx_count = 0;
    }

    return LU_ERR_SUCCESS;
}

void
xe_spine_draw_pass(void)
{
    for (int i = 0; i < XE_MAX_SPINES; ++i) {
        if (g_spines[i].res.state == XE_RS_COMMITED) {
            xe_spine_draw((lu_mat4*)xe_transform_get_global(g_spines[i].node), &g_spines[i]);
        }
    }
}

static void
xe_spine_load(struct xe_res_spine *sp, const char *atlas, const char *skel_json, float scale, const char *idle_ani)
{
    sp->res.state = XE_RS_LOADING;
    struct xe_atlas_entry *atlas_it = g_atlases;
    while (atlas_it) {
        if (!strcmp(atlas, atlas_it->filename)) {
            break;
        }
        atlas_it = atlas_it->next;
    }

    // TODO: Protect from memleaks (I don't care that much)
    if (!atlas_it) {
        atlas_it = malloc(sizeof(*atlas_it));
        if (!atlas_it) {
            lu_log_err("Malloc failed for size: %lu. Aborting spine load.", sizeof(*atlas_it));
            sp->res.state = XE_RS_FAILED;
            return;
        }
        memset(atlas_it, 0, sizeof(*atlas_it));

        if (strlen(atlas) >= XE_SP_FILENAME_LEN) {
            lu_log_err("filename %s not supported: too long.", atlas);
            sp->res.state = XE_RS_FAILED;
            return;
        }

        strcpy(atlas_it->filename, atlas);
        atlas_it->next = g_atlases;
        g_atlases = atlas_it;
    }

    if (!atlas_it->skel_data) {
        atlas_it->atlas = spAtlas_createFromFile(atlas, &atlas_it->img);
        atlas_it->skel_json = spSkeletonJson_create(atlas_it->atlas);
        atlas_it->skel_data = spSkeletonJson_readSkeletonDataFile(atlas_it->skel_json, skel_json);
        if (!atlas_it->skel_data) {
            lu_log_err("%s skeleton data: %s", skel_json, atlas_it->skel_json->error);
            sp->res.state = XE_RS_FAILED;
            return;
        }
    }

    if (!atlas_it->anim_data) {
        atlas_it->anim_data = spAnimationStateData_create(atlas_it->skel_data);
        if (!atlas_it->anim_data) {
            lu_log_err("Could not create animation data.");
            sp->res.state = XE_RS_FAILED;
            return;
        }
    }

    sp->skel = spSkeleton_create(atlas_it->skel_data);
    if (!sp->skel) {
        lu_log_err("Could not create spine skeleton. Aborting spine load.");
        sp->res.state = XE_RS_FAILED;
        return;
    }

    sp->anim = spAnimationState_create(atlas_it->anim_data);
    if (!sp->anim) {
        lu_log_err("Could not create spine animation state. Aborting spine load.");
        sp->res.state = XE_RS_FAILED;
        return;
    }

    /* Init */
    if (scale != 0.0f) {
        sp->skel->scaleX = scale;
        sp->skel->scaleY = scale;
    }

    spSkeleton_setToSetupPose(sp->skel);
    spSkeleton_updateWorldTransform(sp->skel, SP_PHYSICS_UPDATE);
    if (idle_ani && idle_ani[0] != '\0') {
        spAnimationState_setAnimationByName(sp->anim, 0, idle_ani, 1);
    }

    sp->res.state = XE_RS_COMMITED;
}

xe_scene_node
xe_spine_create(const char *atlas, const char *skel_json, float scale, const char *idle_ani)
{
    struct xe_res_spine *sp = NULL;
    for (int i = 0; i < XE_MAX_SPINES; ++i) {
        sp = &g_spines[i];
        if (sp->res.state == XE_RS_FREE) {
            sp->res.state = XE_RS_EMPTY;
            break;
        }
    }

    if (!sp) {
        lu_log_err("Could not create a new spine.");
        return (xe_scene_node){-1};
    }

    struct xe_scene_node_desc desc = {
        .pos_x = 0.0f,
        .pos_y = 0.0f,
        .pos_z = -20.0f,
        .scale = 1.0f,
    };

    sp->node = xe_scene_create_node(&desc);
    xe_spine_load(sp, atlas, skel_json, scale, idle_ani);
    return sp->node;
}

enum { XE_MAX_ATLAS_PAGES = 64 };
static struct xe_atlas_pages_arr {
    char filenames[XE_SP_FILENAME_LEN][XE_MAX_ATLAS_PAGES];
    xe_image img[XE_MAX_ATLAS_PAGES];
} g_atlas_pages;

void _spAtlasPage_createTexture(spAtlasPage *self, const char *path)
{
    for (int i = 0; i < XE_MAX_ATLAS_PAGES; ++i) {
        const char *name = g_atlas_pages.filenames[i];
        if (name[0] == '\0') {
            g_atlas_pages.img[i] = xe_image_load(path, self->pma ? XE_IMG_PREMUL_ALPHA : 0);
            if (strlen(path) >= XE_SP_FILENAME_LEN) {
                lu_log_err("filename %s not supported: too long.", path);
                return;
            }
            strcpy(g_atlas_pages.filenames[i], path);
            self->rendererObject = &g_atlas_pages.img[i];
            return;
        }

        if (!strcmp(name, path)) {
            lu_err_assert(g_atlas_pages.img[i].id != XE_MAX_IMAGES);
            self->rendererObject = &g_atlas_pages.img[i];
            return;
        }
    }

    lu_log_err("Max atlas pages limit reached.");
    lu_err_assert(0);
    /* who cares? here we go again */
    memset(&g_atlas_pages, 0, sizeof(g_atlas_pages));
    if (strlen(path) >= XE_SP_FILENAME_LEN) {
        lu_log_err("filename %s not supported: too long.", path);
    } else {
        strcpy(g_atlas_pages.filenames[0], path);
        g_atlas_pages.img[0] = xe_image_load(path, self->pma ? XE_IMG_PREMUL_ALPHA : 0);
        self->rendererObject = &g_atlas_pages.img[0];
    }
}

void _spAtlasPage_disposeTexture(spAtlasPage *self)
{
    (void)self;
}

char *_spUtil_readFile(const char *path, int *length)
{
    return _spReadFile(path, length);
}
