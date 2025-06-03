#ifndef __XE_SPINE_H__
#define __XE_SPINE_H__

/*
 * Testing the process of integrating external entities to xe_scene.
 * ... and the rendering implementation
 */

#include <xe_scene.h>

typedef struct xe_spine_desc {
	const char *atlas_path;
	const char *skel_path;
	float pos_x;
	float pos_y;
	float scale_x;
	float scale_y;
	float rot;
	float skel_scale;
	const char *anim_name;
	int anim_loop;

} xe_spine_desc;

/* Atlas is the .atlas, not the image. Skeleton can be either jsor or binary
 * scale and idle_ani are optional (for init purposes)
 */
void *xe_spine_get_skel(xe_scene_node node);
void *xe_spine_get_anim(xe_scene_node node);
xe_scene_node xe_spine_create(const char *atlas, const char *skeleton, float scale, const char *idle_ani);
//xe_spine xe_spine_load(const char *atlas, const char *skeleton, float scale, const char *idle_ani);
void xe_spine_animation_pass(float delta_time);
void xe_spine_draw_pass(void);

#endif  /* __XE_SPINE_H__ */
