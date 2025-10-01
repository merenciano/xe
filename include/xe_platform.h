#ifndef XE_PLATFORM_H
#define XE_PLATFORM_H

#include <llulu/lu_time.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct xe_platform_config {
    const char *title;
    int display_w;
    int display_h;
    bool vsync;
    const char *log_filename;
} xe_platform_config;

typedef struct xe_platform {
    const char *name;
    xe_platform_config config;

    /* Inputs (read only) */
    int window_w; /* screen coords */
    int window_h;
    int viewport_w; /* pixels */
    int viewport_h;
    bool close;

    float mouse_x;
    float mouse_y;
    float prev_mouse_x;
    float prev_mouse_y;
    bool mouse_left;
    bool mouse_right;
    bool prev_mouse_left;
    bool prev_mouse_right;

    /* Internal state */
    char window_title[1024];
    void *window;
    void *log_stream;
    void *(*gl_loader)(const char *);

    /* Metrics */
    lu_timestamp begin_timestamp;
    lu_timestamp frame_timestamp;
    int64_t delta_ns;
    uint64_t frame_cnt;

    /* Profiling timers */
    struct {
        int64_t glfw_init;
        int64_t renderer_init;
        int64_t img_load;
        int64_t scene_load;
        int64_t frame_time[256];
        int64_t init_time;
        int64_t shutdown;
        int64_t total;
    } timers_data;
} xe_platform;

bool xe_platform_init(xe_platform *plat, xe_platform_config *config);
float xe_platform_update(void);
void xe_platform_shutdown(void);

int64_t xe_file_mtime(const char *path);
bool xe_file_read(const char *path, void *buf, size_t bufsize, size_t *out_len);

#endif  /* XE_PLATFORM_H */

