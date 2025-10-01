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

#endif /* XE_GFX_INTERNAL_H */

