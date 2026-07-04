#include <xe_scene.h>

#include <llulu/lu_error.h>
#include <llulu/lu_math.h>

#include <stdio.h>

static void tr_print(const float *tr, const char *name)
{
	printf("\n ** %s ** \n", name);
	printf("% 4.1f % 4.1f % 4.1f % 4.1f\n", tr[0], tr[1], tr[2], tr[3]);
	printf("% 4.1f % 4.1f % 4.1f % 4.1f\n", tr[4], tr[5], tr[6], tr[7]);
	printf("% 4.1f % 4.1f % 4.1f % 4.1f\n", tr[8], tr[9], tr[10], tr[11]);
	printf("% 4.1f % 4.1f % 4.1f % 4.1f\n\n", tr[12], tr[13], tr[14], tr[15]);
}

int main(int argc, const char **argv)
{
	xe_scene_node root = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 0.0f,
		.pos_y = 0.0f,
		.pos_z = 0.0f,
		.scale = 1.0f,
		.child_count = 3
	});

	xe_scene_node node_1_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = -4.0f,
		.pos_y = 4.0f,
		.pos_z = 0.0f,
		.scale = 1.0f,
		.child_count = 1
	});

	xe_scene_node node_2_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = -1.0f,
		.pos_y = -1.0f,
		.pos_z = 0.0f,
		.scale = 1.0f,
		.child_count = 0
	});

	xe_scene_node node_1_2 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = -2.0f,
		.pos_y = 2.0f,
		.pos_z = -10.0f,
		.scale = 0.5f,
		.child_count = 0
	});

	xe_scene_node node_1_3 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 2.0f,
		.pos_y = -2.0f,
		.pos_z = -2.0f,
		.scale = 2.0f,
		.child_count = 1
	});

	xe_scene_node node_3_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 0.0f,
		.pos_y = 0.0f,
		.pos_z = 0.0f,
		.scale = 1.0f,
		.child_count = 1
	});

	xe_scene_node node_4_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 0.0f,
		.pos_y = 0.0f,
		.pos_z = 0.0f,
		.scale = 1.0f,
		.child_count = 1
	});

	xe_scene_node node_5_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 10.0f,
		.pos_y = -10.0f,
		.pos_z = 5.0f,
		.scale = 1.0f,
		.child_count = 2
	});

	xe_scene_node node_6_1 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = 1.0f,
		.pos_y = -1.0f,
		.pos_z = 0.0f,
		.scale = 2.0f,
		.child_count = 0
	});

	xe_scene_node node_6_2 = xe_scene_create_node(&(xe_scene_node_desc){
		.pos_x = -1.0f,
		.pos_y = 1.0f,
		.pos_z = 0.0f,
		.scale = 0.5f,
		.child_count = 0
	});

	xe_scene_update_world();

	const float *tr_0_0 = xe_transform_get_global(root);
	const float *tr_1_1 = xe_transform_get_global(node_1_1);
	const float *tr_2_1 = xe_transform_get_global(node_2_1);
	const float *tr_1_2 = xe_transform_get_global(node_1_2);
	const float *tr_1_3 = xe_transform_get_global(node_3_1);
	const float *tr_3_1 = xe_transform_get_global(node_3_1);
	const float *tr_4_1 = xe_transform_get_global(node_4_1);

	tr_print(tr_0_0, "root");
	tr_print(tr_1_1, "node_1_1");
	tr_print(tr_2_1, "node_2_1");
	tr_print(tr_1_2, "node_1_2");

	{
		/* A node with an identity local transform should have the same world values as his parent. */
		for (int i = 0; i < 16; ++i) {
			lu_err_expects(tr_3_1[i] == tr_4_1[i]);
		}

		for (int i = 0; i < 16; ++i) {
			lu_err_expects(tr_3_1[i] == tr_1_3[i]);
		}
	}

	{
		/* 
		* 3_1 has scale 1 and its parent (1_3) has scale 2, so the world transform of 3_1 should have scale 2.
		* 3_1 has local position zeroed, but since its parent has other values, the world position of 3_1 should 
		* be different than zero.
		*/
		lu_err_expects(tr_3_1[0] == 2.0f);
		lu_err_expects(tr_3_1[5] == 2.0f);
		lu_err_expects(tr_3_1[10] == 2.0f);
		lu_err_expects(tr_3_1[12] != 0.0f);
		lu_err_expects(tr_3_1[13] != 0.0f);
		lu_err_expects(tr_3_1[14] != 0.0f);
	}
	

	return 0;
}
