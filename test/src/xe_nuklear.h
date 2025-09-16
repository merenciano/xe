#ifndef XE_NUKLEAR_H
#define XE_NUKLEAR_H

struct xe_platform;
typedef void xe_nkctx;

void        xe_nk_init(struct xe_platform *plat);
xe_nkctx   *xe_nk_new_frame(void);
void        xe_nk_render(void);
void        xe_nk_shutdown(void);
int         xe_nk_overview(xe_nkctx *nk_ctx);

#endif /* XE_NUKLEAR_H */

