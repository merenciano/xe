#include "xe_platform.h"
#include "xe_gfx.h"

#include <llulu/lu_time.h>
#include <llulu/lu_log.h>
#include <llulu/lu_hooks.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
typedef struct _stat xe_stat_t;
static int xe_stat(const char *path, xe_stat_t *st) { return _stat(path, st); }
const char * const g_null_stream = "NUL";
#else /* UNIX */
typedef struct stat xe_stat_t;
static int xe_stat(const char *path, xe_stat_t *st) { return stat(path, st); }
const char * const g_null_stream = "/dev/null";
#endif

static xe_platform g_plat;

static void
xep_log_report()
{
    lu_log(" * Report (%lld frames, %.2f seconds) * ", g_plat.frame_cnt, lu_time_sec(g_plat.timers_data.total));
    lu_log("Init time: %lld ms", lu_time_ms(g_plat.timers_data.init_time));
    lu_log(" - glfw:\t%lld ms", lu_time_ms(g_plat.timers_data.glfw_init));
    lu_log(" - img load:\t%lld ms", lu_time_ms(g_plat.timers_data.img_load));
    lu_log(" - renderer:\t%lld ms", lu_time_ms(g_plat.timers_data.renderer_init));
    lu_log(" - scene:\t%lld ms", lu_time_ms(g_plat.timers_data.scene_load));
    int64_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += g_plat.timers_data.frame_time[i];
    }
    sum /= 256;
    lu_log("Last 256 frame times average: %.2f ms\n", sum / 1000000.0f);
    lu_log("Shutdown: %lld ms\n", lu_time_ms(g_plat.timers_data.shutdown));
}

xe_platform *
xe_platform_create(xe_platform_config *config)
{
    if (g_plat.initialized) {
        lu_log("xep_init call ignored: platform already initialized.");
        return &g_plat;
    }

    g_plat.begin_timestamp = lu_time_get();
    lu_hook_notify(LU_HOOK_PRE_INIT, NULL);

    g_plat.name = "Desktop";
    g_plat.config = *config;
    g_plat.log_stream = fopen(g_plat.config.log_filename, "w");
    if (!g_plat.log_stream) {
        g_plat.log_stream = stdout;
        lu_log_verbose("fopen file %s failed, using stdout instead.", g_plat.config.log_filename);
    }

    lu_err_assert(g_plat.log_stream);

    lu_timestamp timer = lu_time_get();
    if (!glfwInit()) {
        lu_log_panic("Could not init glfw. Aborting...\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    g_plat.window = glfwCreateWindow(g_plat.config.display_w,
                           g_plat.config.display_h,
                           g_plat.config.title,
                           NULL, NULL);
    GLFWwindow *win = g_plat.window;
    if (!win) {
        glfwTerminate();
        lu_log_panic("Could not open glfw window. Aborting...\n");
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
    xe_gfx_init(&(xe_gfx_config){
        .gl_loader = (void *(*)(const char *))glfwGetProcAddress,
        .canvas = {.w = canv_w, .h = canv_h},
        .vert_shader_path = "./assets/vert.glsl",
        .frag_shader_path = "./assets/frag.glsl"
    });

    elapsed = lu_time_elapsed(timer);
    g_plat.timers_data.renderer_init = elapsed;
    g_plat.frame_timestamp = lu_time_get();

    g_plat.initialized = true;
    lu_hook_notify(LU_HOOK_POST_INIT, &g_plat);
    return &g_plat;
}

float
xe_platform_update(void)
{
    xe_gfx_render();
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
    lu_hook_notify(LU_HOOK_PRE_SHUTDOWN, &g_plat);
    if (!g_plat.initialized) {
        return;
    }

    lu_timestamp start = lu_time_get();
    xe_gfx_shutdown();
    glfwDestroyWindow(g_plat.window);
    glfwTerminate();
    g_plat.timers_data.shutdown = lu_time_elapsed(start);
    g_plat.timers_data.total = lu_time_elapsed(g_plat.begin_timestamp);
    xep_log_report();
    fclose(g_plat.log_stream);
    g_plat.initialized = false;
    lu_hook_notify(LU_HOOK_POST_SHUTDOWN, &g_plat);
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
        lu_log_err("%s not found.\n", path);
        return LU_ERR_FILE;
    }

    if (err == 22) {
        lu_log_err("Invalid arg in stat call for file %s.\n", path);
        return LU_ERR_BADARG;
    }

    return st.st_mtime;
}

bool
xe_file_read(const char *path, void *buf, size_t bufsize, size_t *out_len)
{
    lu_err_assert(buf && out_len && bufsize);
    FILE *f = fopen(path, "r");
    if (!f) {
        lu_log_err("Could not open file %s.\n", path);
        *out_len = 0;
        return false;
    }

    *out_len = fread(buf, 1, bufsize, f);
    int eof = feof(f);
    fclose(f);
    return eof;
}
