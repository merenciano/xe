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

static const char *const XE_PLATFORM_NAME = "glfw3";
static xe_platform *pl;

static void
xep_log_report()
{
    lu_err_assert(pl);
    lu_log(" * Report (%lld frames, %.2f seconds) * ", pl->frame_cnt, lu_time_sec(pl->timers_data.total));
    lu_log("Init time: %lld ms", lu_time_ms(pl->timers_data.init_time));
    lu_log(" - glfw:\t%lld ms", lu_time_ms(pl->timers_data.glfw_init));
    lu_log(" - img load:\t%lld ms", lu_time_ms(pl->timers_data.img_load));
    lu_log(" - renderer:\t%lld ms", lu_time_ms(pl->timers_data.renderer_init));
    lu_log(" - scene:\t%lld ms", lu_time_ms(pl->timers_data.scene_load));
    int64_t sum = 0;
    for (int i = 0; i < 256; ++i) {
        sum += pl->timers_data.frame_time[i];
    }
    sum /= 256;
    lu_log("Last 256 frame times average: %.2f ms\n", sum / 1000000.0f);
    lu_log("Shutdown: %lld ms\n", lu_time_ms(pl->timers_data.shutdown));
}

bool
xe_platform_init(xe_platform *platform, xe_platform_config *config)
{
    lu_err_assert(!pl && platform);
    lu_err_assert(config);
    pl = platform;
    if (pl->name == XE_PLATFORM_NAME) {
        lu_log("xep_init call ignored: platform already initialized.");
        return false; 
    }

    pl->begin_timestamp = lu_time_get();
    lu_hook_notify(LU_HOOK_PRE_INIT, NULL);

    pl->name = XE_PLATFORM_NAME;
    pl->config = *config;
    if (!pl->config.log_filename || *pl->config.log_filename == '\0') {
        pl->log_stream = stdout;
    } else {
        pl->log_stream = fopen(pl->config.log_filename, "w");
        if (!pl->log_stream) {
            pl->log_stream = stdout;
            lu_log_verbose("fopen file %s failed, using stdout instead.", pl->config.log_filename);
        }
    }

    lu_err_assert(pl->log_stream);

    lu_timestamp timer = lu_time_get();
    if (!glfwInit()) {
        lu_log_panic("Could not init glfw. Aborting...\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    pl->window = glfwCreateWindow(pl->config.display_w,
                           pl->config.display_h,
                           pl->config.title,
                           NULL, NULL);
    GLFWwindow *win = pl->window;
    if (!win) {
        glfwTerminate();
        lu_log_panic("Could not open glfw window. Aborting...\n");
        return false;
    }

    glfwMakeContextCurrent(win);
    glfwSwapInterval((int)pl->config.vsync);

    pl->gl_loader = (void *(*)(const char *))glfwGetProcAddress;
    glfwGetFramebufferSize(win, &pl->viewport_w, &pl->viewport_h);
    glfwGetWindowSize(win, &pl->window_w, &pl->window_h);

    int64_t elapsed = lu_time_elapsed(timer);
    pl->timers_data.glfw_init = elapsed;


    timer = lu_time_get();
    elapsed = lu_time_elapsed(timer);
    pl->timers_data.renderer_init = elapsed;
    pl->frame_timestamp = lu_time_get();
    lu_hook_notify(LU_HOOK_POST_INIT, pl);
    return true;
}

float
xe_platform_update(void)
{
    lu_err_assert(pl && pl->name == XE_PLATFORM_NAME);
    GLFWwindow *win = pl->window;
    glfwSwapBuffers(win);
    pl->delta_ns = lu_time_elapsed(pl->frame_timestamp);
    pl->frame_timestamp = lu_time_get();
    pl->timers_data.frame_time[pl->frame_cnt % 256] = pl->delta_ns;
    ++pl->frame_cnt;
    sprintf(pl->window_title, "%s  |  %f fps", pl->config.title, 1.0f / lu_time_sec(pl->delta_ns));
    glfwSetWindowTitle(win, pl->window_title);
    glfwPollEvents();
    glfwGetFramebufferSize(win, &pl->viewport_w, &pl->viewport_h);
    glfwGetWindowSize(win, &pl->window_w, &pl->window_h);
    double x, y;
    glfwGetCursorPos(win, &x, &y);
    pl->prev_mouse_x = pl->mouse_x;
    pl->prev_mouse_y = pl->mouse_y;
    pl->mouse_x = (float)x;
    pl->mouse_y = (float)y;
    pl->close = glfwWindowShouldClose(win);
    pl->prev_mouse_left = pl->mouse_left;
    pl->prev_mouse_right = pl->mouse_right;
    pl->mouse_left = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT);
    pl->mouse_right = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT);

    return lu_time_sec(pl->delta_ns);
}

void
xe_platform_shutdown(void)
{
    lu_hook_notify(LU_HOOK_PRE_SHUTDOWN, pl);
    lu_timestamp start = lu_time_get();
    if (!pl || pl->name != XE_PLATFORM_NAME) {
        goto shutdown_skip;
    }

    glfwDestroyWindow(pl->window);
    glfwTerminate();
    pl->name = "";

shutdown_skip:
    pl->timers_data.shutdown = lu_time_elapsed(start);
    pl->timers_data.total = lu_time_elapsed(pl->begin_timestamp);
    xep_log_report();
    fclose(pl->log_stream);
    lu_hook_notify(LU_HOOK_POST_SHUTDOWN, pl);
}

void
xe_log_ex(const char *tag, const char *file, int line, ...)
{
    va_list args;
    va_start(args, line);
    const char *fmt = va_arg(args, const char*);
    fprintf(pl->log_stream, "[%s] %s(%d): ", tag, file, line);
    vfprintf(pl->log_stream, fmt, args);
    fputc('\n', pl->log_stream);
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

