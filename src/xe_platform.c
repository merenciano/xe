#include "xe_platform.h"
#include "xe_renderer.h"
#include "llulu/lu_time.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
typedef struct _stat xe_stat_t;
static inline int xe_stat(const char *path, xe_stat_t *st) { return _stat(path, st); }
const char * const g_null_stream = "NUL";
#else /* UNIX */
typedef struct stat xe_stat_t;
static inline int xe_stat(const char *path, xe_stat_t *st) { return stat(path, st); }
const char * const g_null_stream = "/dev/null";
#endif

static xe_platform g_plat;

static inline void
xep_log_report()
{
    XE_LOG(" * Report (%lld frames, %.2f seconds) * ", g_plat.frame_cnt, lu_time_sec(g_plat.timers_data.total));
    XE_LOG("Init time: %lld ms", lu_time_ms(g_plat.timers_data.init_time));
    XE_LOG(" - glfw:\t\t%lld ms", lu_time_ms(g_plat.timers_data.glfw_init));
    XE_LOG(" - img load:\t%lld ms", lu_time_ms(g_plat.timers_data.img_load));
    XE_LOG(" - renderer:\t%lld ms", lu_time_ms(g_plat.timers_data.renderer_init));
    XE_LOG(" - scene:\t\t%lld ms", lu_time_ms(g_plat.timers_data.scene_load));
    int64_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += g_plat.timers_data.frame_time[i];
    }
    sum /= 256;
    XE_LOG("Last 256 frame times average: %.2f ms\n", sum / 1000000.0f);
    XE_LOG("Shutdown: %lld ms\n", lu_time_ms(g_plat.timers_data.shutdown));
}

xe_platform *
xe_platform_create(xe_platform_config *config)
{
    if (g_plat.initialized) {
        XE_LOG("xep_init call ignored: platform already initialized.");
        return &g_plat;
    }

    g_plat.begin_timestamp = lu_time_get();

    g_plat.name = "Desktop";
    g_plat.config = *config;
    g_plat.log_stream = fopen(g_plat.config.log_filename, "w");
    if (!g_plat.log_stream) {
        g_plat.log_stream = stdout;
        XE_LOG("fopen file %s failed, using stdout instead.", g_plat.config.log_filename);
    }

    xe_assert(g_plat.log_stream);

    lu_timestamp timer = lu_time_get();
    if (!glfwInit()) {
        XE_LOG_PANIC("Could not init glfw. Aborting...\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);

    g_plat.window = glfwCreateWindow(g_plat.config.display_w,
                           g_plat.config.display_h,
                           g_plat.config.title,
                           NULL, NULL);
    GLFWwindow *win = g_plat.window;
    if (!win) {
        glfwTerminate();
        XE_LOG_PANIC("Could not open glfw window. Aborting...\n");
        return NULL;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval((int)g_plat.config.vsync);

    int64_t elapsed = lu_time_elapsed(timer);
    g_plat.timers_data.glfw_init = elapsed;
    timer = lu_time_get();

    // INIT renderer
    int canv_w, canv_h;
    glfwGetFramebufferSize(win, &canv_w, &canv_h);
    xe_rend_init(&(xe_rend_config){
        .gl_loader = (void *(*)(const char *))glfwGetProcAddress,
        .canvas = {.w = canv_w, .h = canv_h},
        .seconds_between_shader_file_changed_checks = 2.0f,
        .vert_shader_path = "./assets/vert.glsl",
        .frag_shader_path = "./assets/frag.glsl"
    });

    elapsed = lu_time_elapsed(timer);
    g_plat.timers_data.renderer_init = elapsed;
    g_plat.frame_timestamp = lu_time_get();

    g_plat.initialized = true;
    return &g_plat;
}

float
xe_platform_update(void)
{
    xe_rend_render();
    GLFWwindow *win = g_plat.window;
    glfwSwapBuffers(win);
    g_plat.delta_ns = lu_time_elapsed(g_plat.frame_timestamp);
    g_plat.frame_timestamp = lu_time_get();
    g_plat.timers_data.frame_time[g_plat.frame_cnt % 256] = g_plat.delta_ns;
    ++g_plat.frame_cnt;
    sprintf(g_plat.window_title, "%s  |  %f fps", g_plat.config.title, 1.0f / lu_time_sec(g_plat.delta_ns));
    glfwSetWindowTitle(win, g_plat.window_title);
    glfwPollEvents();
    glfwGetWindowSize(win, &g_plat.config.display_w, &g_plat.config.display_w);
    int canvas_w, canvas_h;
    glfwGetFramebufferSize(win, &canvas_w, &canvas_h);
    xe_rend_canvas_resize(canvas_w, canvas_h);
    double x, y;
    glfwGetCursorPos(win, &x, &y);
    g_plat.mouse_x = x;
    g_plat.mouse_y = y;
    g_plat.close = glfwWindowShouldClose(win);
    return lu_time_sec(g_plat.delta_ns);
}

void
xe_platform_shutdown()
{
    if (!g_plat.initialized) {
        return;
    }

    lu_timestamp start = lu_time_get();
    xe_rend_shutdown();
    glfwDestroyWindow(g_plat.window);
    glfwTerminate();
    g_plat.timers_data.shutdown = lu_time_elapsed(start);
    g_plat.timers_data.total = lu_time_elapsed(g_plat.begin_timestamp);
    xep_log_report();
    fclose(g_plat.log_stream);
    g_plat.initialized = false;
}

void
xe_log_ex(const char *tag, const char *file, int line, ...)
{
    va_list args;
    va_start(args, line);
    const char *fmt = va_arg(args, const char*);
    fprintf(g_plat.log_stream, "[%s] %s(%d): ", tag, file, line);
    vfprintf(g_plat.log_stream, fmt, args);
    fputc('\n', g_plat.log_stream);
    va_end(args);
}

int64_t
xe_file_mtime(const char *path)
{
    xe_stat_t st;
    int err = xe_stat(path, &st);
    if (err == -1) {
        printf("%s not found.\n", path);
        return XE_ERR_PLAT_FILE;
    } else if (err == 22) {
        printf("Invalid arg in stat call for file %s.\n", path);
        return XE_ERR_ARG;
    }

    return st.st_mtime;
}
