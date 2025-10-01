#ifndef XE_NUKLEAR_H
#define XE_NUKLEAR_H

//#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#if 0
#define NK_INCLUDE_DEFAULT_ALLOCATOR     /*  TODO: remove */
#define NK_INCLUDE_DEFAULT_FONT
#endif
#define NK_INCLUDE_FONT_BAKING

struct xe_platform;
void xe_nk_init(const struct xe_platform *plat);
struct nk_context *xe_nk_new_frame(void);
void xe_nk_render(void);
void xe_nk_shutdown(void);

#endif  /* XE_NUKLEAR_H */

