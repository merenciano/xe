#include "orxata.h"
#include "spine/AnimationState.h"
#include "spine/Attachment.h"
#include "spine/Skeleton.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>
#include <llulu/lu_time.h>
#include <llulu/lu_math.h>
#include <spine/spine.h>

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

struct {
    int64_t frame_count;
    int64_t glfw_init_ns;
    int64_t orx_init_ns;
    int64_t stb_img_load_ns;
    int64_t frame_time[256];
    int64_t init_time_ns;
    int64_t shutdown_time_ns;
    int64_t total_time_ns;
} timer_data;

void print_timer_data();

GLFWwindow *g_window;

int main(int argc, char **argv)
{
    lu_timestamp start_time = lu_time_get();
    lu_timestamp timer = start_time;
    orx_config_t cfg = {
        .gl_loader = NULL,
        .title = "Fartó",
        .canvas_width = 1280,
        .canvas_height = 860,
        .vert_shader_path = "./assets/vert.glsl",
        .frag_shader_path = "./assets/frag.glsl",
        .seconds_between_shader_file_changed_checks = 1.0f,
    };

    if (!glfwInit()) {
        printf("Could not init glfw. Aborting...\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(cfg.canvas_width, cfg.canvas_height, cfg.title, NULL, NULL);
    if (!g_window) {
        glfwTerminate();
        printf("Could not open glfw window. Aborting...\n");
        return 1;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(0);

    cfg.gl_loader = (void *(*)(const char *))glfwGetProcAddress;

    int64_t elapsed = lu_time_elapsed(timer);
    timer_data.glfw_init_ns = elapsed;
    timer = lu_time_get();

    orx_image_t owl_atlas = orx_load_image("./assets/owl.png");
    orx_image_t tex_test1 = orx_load_image("./assets/default_tex.png");
    orx_image_t tex_test2 = orx_load_image("./assets/tex_test.png");

    elapsed = lu_time_elapsed(timer);
    timer_data.stb_img_load_ns = elapsed;
    timer = lu_time_get();
    
    orx_init(&cfg);

    orx_texture_set(owl_atlas.tex, owl_atlas.data);
    stbi_image_free(owl_atlas.data);
    owl_atlas.data = NULL;

    orx_texture_set(tex_test1.tex, tex_test1.data);
    stbi_image_free(tex_test1.data);
    tex_test1.data = NULL;

    orx_texture_set(tex_test2.tex, tex_test2.data);
    stbi_image_free(tex_test2.data);
    tex_test2.data = NULL;

    orx_vertex_t vertices[4] = {
        {.x = -1.0f, .y = -1.0f,
         .u = 0.0f, .v = 0.0f,
         .color = 0xFFFF00FF},
        {.x = -1.0f, .y = 1.0f,
         .u = 0.0f, .v = 1.0f,
         .color = 0xFF00FFFF},
        {.x = 1.0f, .y = 1.0f,
         .u = 1.0f, .v = 1.0f,
         .color = 0xFFFFFF00},
        {.x = 1.0f, .y = -1.0f,
         .u = 1.0f, .v = 0.0f,
         .color = 0xFF00FF00}
    };
    orx_index_t indices[6] = {0, 1, 2, 0, 2, 3};

    orx_node_t nodes[4];
#ifdef ORX_DEBUG
    nodes[0].name = "Node_0";
    nodes[1].name = "Node_1";
    nodes[2].name = "Node_2";
    nodes[3].name = "Node_3";
#endif
    nodes[0].pos_x = 640.0f * 0.5f;
    nodes[0].pos_y = 430.0f * 0.5f;
    nodes[0].scale_x = 200.0f;
    nodes[0].scale_y = 200.0f;
    nodes[0].shape = orx_mesh_add(vertices, 4, indices, 6);
    nodes[0].shape.texture = tex_test1.tex;
    nodes[1].pos_x = 640.0f * 0.5f;
    nodes[1].pos_y = 430.0f * 1.5f;
    nodes[1].scale_x = 100.0f;
    nodes[1].scale_y = 100.0f;
    nodes[1].shape = nodes[0].shape;
    nodes[1].shape.texture = tex_test1.tex;
    nodes[2].pos_x = 640.0f * 1.5f;
    nodes[2].pos_y = 430.0f * 1.5f;
    nodes[2].scale_x = 100.0f;
    nodes[2].scale_y = 100.0f;
    nodes[2].shape = nodes[0].shape;
    nodes[2].shape.texture = tex_test2.tex;
    nodes[3].pos_x = 640.0f * 1.5f;
    nodes[3].pos_y = 430.0f * 0.5f;
    nodes[3].scale_x = 100.0f;
    nodes[3].scale_y = 100.0f;
    nodes[3].shape = nodes[0].shape;

    elapsed = lu_time_elapsed(timer);
    timer_data.orx_init_ns = elapsed;

    spBone_setYDown(-1);
    spAtlas *sp_atlas = spAtlas_createFromFile("./assets/owl.atlas", &owl_atlas);
    spSkeletonJson *skel_json = spSkeletonJson_create(sp_atlas);
    spSkeletonData *skel_data = spSkeletonJson_readSkeletonDataFile(skel_json, "./assets/owl-pro.json");
    if (!skel_data) {
        printf("%s\n", skel_json->error);
        return 1;
    }
    spAnimationStateData *anim_data = spAnimationStateData_create(skel_data);
    orx_spine_t spine_node = {.skel = spSkeleton_create(skel_data), .anim = spAnimationState_create(anim_data)};
    spSkeleton_setToSetupPose(spine_node.skel);
    int total_vertices = 0;
    int total_indices = 0;
    for (int i = 0; i < skel_data->skinsCount; ++i) {
        for (spSkinEntry* entry = spSkin_getAttachments(skel_data->skins[i]); entry; entry = entry->next) {
            if (entry->attachment) {
                switch (entry->attachment->type) {
                case SP_ATTACHMENT_MESH:
                    total_vertices += (((spMeshAttachment*)entry->attachment)->super.worldVerticesLength / 2);
                    total_indices += ((spMeshAttachment*)entry->attachment)->trianglesCount;
                    break;
                case SP_ATTACHMENT_REGION:
                    total_vertices += 4;
                    total_indices += 6;
                    break;
                default:
                    break;
                }
            }
        }
    }

    spine_node.node.shape = orx_mesh_add(NULL, total_vertices, NULL, total_indices);
    spine_node.node.shape.texture = owl_atlas.tex;
    spSkeleton_updateWorldTransform(spine_node.skel, SP_PHYSICS_UPDATE);

    spine_node.node.pos_x = 640.0f;
    spine_node.node.pos_y = 640.0f;
    spine_node.node.scale_x = 0.8f;
    spine_node.node.scale_y = 0.8f;
    spAnimationState_setAnimationByName(spine_node.anim, 0, "idle", true);
    spAnimationState_setAnimationByName(spine_node.anim, 1, "blink", true);

    spTrackEntry *left = spAnimationState_setAnimationByName(spine_node.anim, 2, "left", true);
	spTrackEntry *right = spAnimationState_setAnimationByName(spine_node.anim, 3, "right", true);
	spTrackEntry *up = spAnimationState_setAnimationByName(spine_node.anim, 4, "up", true);
	spTrackEntry *down = spAnimationState_setAnimationByName(spine_node.anim, 5, "down", true);

	left->alpha = 0;
	left->mixBlend = SP_MIX_BLEND_ADD;
	right->alpha = 0;
	right->mixBlend = SP_MIX_BLEND_ADD;
	up->alpha = 0;
	up->mixBlend = SP_MIX_BLEND_ADD;
	down->alpha = 0;
	down->mixBlend = SP_MIX_BLEND_ADD;

    timer_data.init_time_ns = lu_time_elapsed(start_time);
    timer = lu_time_get();

    while(!glfwWindowShouldClose(g_window)) {

        double dx, dy;
        glfwGetCursorPos(g_window, &dx, &dy);
        float x = dx / cfg.canvas_width;
        float y = dy / cfg.canvas_height;
        left->alpha = (lu_maxf(x, 0.5f) - 0.5f) * 1.0f;
        right->alpha = (0.5f - lu_minf(x, 0.5f)) * 1.0f;
        down->alpha = (lu_maxf(y, 0.5f) - 0.5f) * 1.0f;
        up->alpha = (0.5f - lu_minf(y, 0.5f)) * 1.0f;

        spSkeleton_setToSetupPose(spine_node.skel);
        orx_spine_update(&spine_node, lu_time_sec(elapsed));

        float curr_time_sec = lu_time_sec(lu_time_elapsed(start_time));
        nodes[0].scale_x = 100.0f + sinf(curr_time_sec) * 50.0f;
        nodes[0].scale_y = 100.0f + cosf(curr_time_sec) * 50.0f;

        orx_draw_node(&nodes[0]);
        orx_draw_node(&nodes[1]);
        orx_draw_node(&nodes[2]);
        orx_draw_node(&nodes[3]);
        orx_spine_draw(&spine_node);

        orx_render();

        glfwSwapBuffers(g_window);
        glfwPollEvents();

        elapsed = lu_time_elapsed(timer);
        timer = lu_time_get();
        char buf[1024];
        sprintf(buf, "%s  |  %f fps", cfg.title, 1.0f / lu_time_sec(elapsed));
        glfwSetWindowTitle(g_window, buf);
        timer_data.frame_time[timer_data.frame_count % 256] = elapsed;
        timer_data.frame_count++;
    }

    timer = lu_time_get();
    glfwDestroyWindow(g_window);
    glfwTerminate();
    timer_data.shutdown_time_ns = lu_time_elapsed(timer);
    timer_data.total_time_ns = lu_time_elapsed(start_time);

#ifdef ORX_VERBOSE
    print_timer_data();
#endif
    return 0;
}

static inline void print_timer_data()
{
    printf(" * Report (%lld frames, %.2f seconds) * \n", timer_data.frame_count, lu_time_sec(timer_data.total_time_ns));
    printf("Init time: %lld ms\n", lu_time_ms(timer_data.init_time_ns));
    printf(" - glfw:\t%lld ms\n", lu_time_ms(timer_data.glfw_init_ns));
    printf(" - stb_img:\t%lld ms\n", lu_time_ms(timer_data.stb_img_load_ns));
    printf(" - orxata:\t%lld ms\n", lu_time_ms(timer_data.orx_init_ns));
    int64_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += timer_data.frame_time[i];
    }
    sum /= 256;
    double sum_d = sum;
    printf("Last 256 frame times average: %.2f ms\n", sum / 1000000.0f);
    printf("Shutdown: %lld ms\n", lu_time_ms(timer_data.shutdown_time_ns));
}
