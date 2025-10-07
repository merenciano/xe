#ifndef XE_NUKLEAR_H
#define XE_NUKLEAR_H

#ifdef NK_NUKLEAR_H_
#error "nuklear.h has to be included AFTER xe_nuklear.h"
#endif

#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING

struct xe_platform;
void xe_nk_init(const struct xe_platform *plat);
struct nk_context *xe_nk_new_frame(void);
void xe_nk_render(void);
void xe_nk_shutdown(void);

#endif  /* XE_NUKLEAR_H */
