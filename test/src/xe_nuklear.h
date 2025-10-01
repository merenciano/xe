#ifndef XE_NUKLEAR_H
#define XE_NUKLEAR_H

struct xe_platform;

void                xe_nk_init(const struct xe_platform *plat);
struct nk_context  *xe_nk_new_frame(void);
void                xe_nk_render(void);
void                xe_nk_shutdown(void);

#endif /* XE_NUKLEAR_H */

