#include <xe_gfx.h>

typedef struct xe_mesh {
    int base_vtx;
    int first_idx;
    int idx_count;
} xe_mesh;

xe_mesh xe_mesh_add(const void *vert, size_t vert_size, const void *indices, size_t indices_size);
int xe_material_add(const xe_gfx_material *mat);
void xe_cmd_add(xe_mesh mesh, int draw_id);

