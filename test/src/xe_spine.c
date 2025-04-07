#include "xe_spine.h"
#include "xe.h"
#include "xe_renderer.h"
#include "xe_resource.h"

#include <xe_platform.h>

#include <spine/spine.h>
#include <spine/extension.h>

enum {XE_SP_FILENAME_LEN = 256};

struct xe_res_spine {
    struct xe_resource res;
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

enum { XE_MAX_SPINES = 32 };
static struct xe_res_spine g_spines[XE_MAX_SPINES];

static inline struct xe_res_spine *
xe_spine_ptr(xe_spine sp)
{
    uint16_t i = xe_res_index(sp.id);
    return i < XE_MAX_SPINES ? &g_spines[i] : NULL;
}

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
    xe_rend_vtx vert[XE_SPBATCH_VTX_CAP];
    xe_rend_idx indices[XE_SPBATCH_IDX_CAP << 1];
    int64_t vtx_count;
    int64_t idx_count;
    xe_rend_material material;
    xe_rend_material curr_mat;
};

/* TODO: Go back to colored vertices but keep dark color with the materials, so Additive blend can zero its alpha
 * without using another draw indirect command for the index. */
void
xe_spine_draw(struct xe_res_spine *self, xe_mat4 *tr)
{
    static struct slot_batch current_batch;
    static spSkeletonClipping *g_clipper = NULL;
	if (!g_clipper) {
		g_clipper = spSkeletonClipping_create();
	}

    xe_assert(tr);

    current_batch.material.model = *tr;
    current_batch.curr_mat.model = *tr;
    current_batch.curr_mat.pma = true;
    current_batch.vtx_count = 0;
    current_batch.idx_count = 0;

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
			const struct xe_res_image *pimg = xe_image_ptr(*((xe_image*)((spAtlasRegion *)region->rendererObject)->page->rendererObject));
            current_batch.curr_mat.pma = pimg->flags & XE_IMG_PREMUL_ALPHA;
			current_batch.curr_mat.tex = pimg->tex;
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
			const struct xe_res_image *pimg = xe_image_ptr(*((xe_image*)((spAtlasRegion *)mesh->rendererObject)->page->rendererObject));
            current_batch.curr_mat.pma = pimg->flags & XE_IMG_PREMUL_ALPHA;
			current_batch.curr_mat.tex = pimg->tex;
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


        current_batch.curr_mat.darkcolor = slot->darkColor ? (xe_vec4){ slot->darkColor->r, slot->darkColor->g, slot->darkColor->b, 1.0f} : (xe_vec4){0.0f, 0.0f, 0.0f, 1.0f};
        //current_batch.curr_mat.color = (xe_vec4){
        //    .x = sk->color.r * slot->color.r * attach_color->r,
        //    .y = sk->color.g * slot->color.g * attach_color->g,
        ///    .z = sk->color.b * slot->color.b * attach_color->b,
        //    .w = slot->data->blendMode == SP_BLEND_MODE_ADDITIVE ? 0.0f : sk->color.a * slot->color.a * attach_color->a
        //};

        if (((current_batch.vtx_count << 1) > XE_SPBATCH_VTX_CAP) ||
            ((current_batch.idx_count << 1) > XE_SPBATCH_IDX_CAP) ||
            (current_batch.vtx_count && (memcmp(&current_batch.material.darkcolor, &current_batch.curr_mat.darkcolor, sizeof(xe_vec4))))) {
            xe_rend_mesh mesh = xe_rend_mesh_add(current_batch.vert, current_batch.vtx_count * sizeof(xe_rend_vtx),
                                                 current_batch.indices, current_batch.idx_count * sizeof(xe_rend_idx));
            current_batch.material.model = *tr;
            xe_rend_draw_id draw = xe_rend_material_add(current_batch.material);
            xe_rend_submit(mesh, draw);
            current_batch.idx_count = 0;
            current_batch.vtx_count = 0;
        }

        switch (slot->data->blendMode) {
            case SP_BLEND_MODE_NORMAL:
            case SP_BLEND_MODE_ADDITIVE:
                current_batch.material.darkcolor = current_batch.curr_mat.darkcolor;
                current_batch.material.tex = current_batch.curr_mat.tex;
                memcpy(&current_batch.vert[current_batch.vtx_count], vertices, slot_vtx_count * sizeof(xe_rend_vtx));
                memcpy(&current_batch.indices[current_batch.idx_count], indices, slot_idx_count * sizeof(xe_rend_idx));
                current_batch.vtx_count += slot_vtx_count;
                current_batch.idx_count += slot_idx_count;
                break;

            case SP_BLEND_MODE_MULTIPLY:
            	XE_LOG_VERBOSE("Multiply alpha blending not implemented.");
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

    if (current_batch.vtx_count) {
        xe_rend_mesh mesh = xe_rend_mesh_add(current_batch.vert, current_batch.vtx_count * sizeof(xe_rend_vtx),
                                                current_batch.indices, current_batch.idx_count * sizeof(xe_rend_idx));
        current_batch.material.model = *tr;
        xe_rend_draw_id draw = xe_rend_material_add(current_batch.material);
        xe_rend_submit(mesh, draw);
        current_batch.idx_count = 0;
        current_batch.vtx_count = 0;
    }
}

void
xe_spine_draw_pass()
{
    for (int i = 0; i < XE_MAX_SPINES; ++i) {
        if (g_spines[i].res.state == XE_RS_COMMITED) {
            xe_spine_draw(g_spines + i, (void*)0);
        }
    }
}

xe_spine
xe_spine_load(const char *atlas, const char *skel_json, float scale, const char *idle_ani)
{
    xe_spine h = {.id = XE_MAX_SPINES};
    // TODO arg checks and ext

    struct xe_res_spine *sp = NULL;
    for (int i = 0; i < XE_MAX_SPINES; ++i) {
        sp = &g_spines[i];
        if (sp->res.state == XE_RS_FREE) {
            sp->res.state = XE_RS_EMPTY;
            uint16_t ver = sp->res.version++;
            h.id = xe_res_handle_gen(ver, i);
            break;
        }
    }

    if (sp) {
        sp->res.state = XE_RS_LOADING;

        struct xe_atlas_entry *atlas_it = g_atlases;
        while (atlas_it) {
            if (!strcmp(atlas, atlas_it->filename)) {
                break;
            }
            atlas_it = atlas_it->next;
        }

        // TODO: Protect from memleaks (I don't care that much)
        if (!atlas_it || !atlas_it->skel_data || !atlas_it->anim_data) {
            atlas_it = malloc(sizeof(*atlas_it));
            if (strlen(atlas) >= XE_SP_FILENAME_LEN) {
                XE_LOG_ERR("filename %s not supported: too long.", atlas);
                goto err_ret;
            }
            strcpy(atlas_it->filename, atlas);
            atlas_it->next = g_atlases;
            g_atlases = atlas_it;
            atlas_it->atlas = spAtlas_createFromFile(atlas, &atlas_it->img);
            atlas_it->skel_json = spSkeletonJson_create(atlas_it->atlas);
            atlas_it->skel_data = spSkeletonJson_readSkeletonDataFile(atlas_it->skel_json, skel_json);
            if (!atlas_it->skel_data) {
                XE_LOG_ERR("%s skeleton data: %s", skel_json, atlas_it->skel_json->error);
                goto err_ret;
            }
            atlas_it->anim_data = spAnimationStateData_create(atlas_it->skel_data);
        }

        sp->skel = spSkeleton_create(atlas_it->skel_data);
        sp->anim = spAnimationState_create(atlas_it->anim_data);

        /* Init */
        if (scale != 0.0f) {
            sp->skel->scaleX = scale;
            sp->skel->scaleY = scale;
        }

        spSkeleton_setToSetupPose(sp->skel);
        spSkeleton_updateWorldTransform(sp->skel, SP_PHYSICS_UPDATE);
        if (idle_ani) {
            spAnimationState_setAnimationByName(sp->anim, 0, idle_ani, true);
        }

        sp->res.state = XE_RS_COMMITED;
    }

    return h; // sp could be NULL

err_ret:
    sp->res.state = XE_RS_FAILED;
    return h;
}

xe_scene_node
xe_spine_create(const char *atlas, const char *skel_json, float scale, const char *idle_ani)
{
    xe_spine res;
    xe_scene_node node = xe_scene_node_create(&res);
    res.mask = XE_RP_DRAWABLE;
    res.
}

static struct xe_atlas_pages_arr {
    enum { XE_MAX_ATLAS_PAGES = 64 };
    char filenames[XE_SP_FILENAME_LEN][XE_MAX_ATLAS_PAGES];
    xe_image img[XE_MAX_ATLAS_PAGES];
} g_atlas_pages;

void _spAtlasPage_createTexture(spAtlasPage *self, const char *path)
{
    for (int i = 0; i < XE_MAX_ATLAS_PAGES; ++i) {
        const char *name = g_atlas_pages.filenames[i];
        if (!name) {
            g_atlas_pages.img[i] = xe_image_load(path, self->pma ? XE_IMG_PREMUL_ALPHA : 0);
            if (strlen(path) >= XE_SP_FILENAME_LEN) {
                XE_LOG_ERR("filename %s not supported: too long.", path);
                return;
            }
            strcpy(g_atlas_pages.filenames[i], path);
            self->rendererObject = &g_atlas_pages.img[i];
            return;
        }

        if (!strcmp(name, path)) {
            assert(g_atlas_pages.img[i].id != XE_MAX_IMAGES);
            self->rendererObject = &g_atlas_pages.img[i];
            return;
        }
    }

    XE_LOG_ERR("Max atlas pages limit reached.");
    xe_assert(0);
    /* who cares? here we go again */
    memset(&g_atlas_pages, 0, sizeof(g_atlas_pages));
    if (strlen(path) >= XE_SP_FILENAME_LEN) {
        XE_LOG_ERR("filename %s not supported: too long.", path);
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
