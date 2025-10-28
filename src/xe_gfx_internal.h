#ifndef XE_GFX_INTERNAL_H
#define XE_GFX_INTERNAL_H

#include <xe_gfx.h>

typedef struct xe_mesh {
    int base_vtx;
    int first_idx;
    int idx_count;
} xe_mesh;

extern lu_mat4 view_projection;

xe_mesh xe_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size);

int xe_material_add(const xe_gfx_material *mat);
bool xe_drawcmd_add(xe_mesh mesh, int draw_id);

/* low level api */
void xe__vtxbuf_remaining(void **out_vtx, size_t *out_vtx_rem, size_t *out_first_vtx, void **out_idx, size_t *out_idx_rem, size_t *out_first_idx);
void xe__vtxbuf_push_nocheck(size_t vtx_bytes, size_t idx_bytes);

#endif /* XE_GFX_INTERNAL_H */

