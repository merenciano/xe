#ifndef __XE_PLATFORM_H__
#define __XE_PLATFORM_H__

#include "xe_renderer.h"
#include <llulu/lu_time.h>

#include <stdint.h>
#include <stdbool.h>

#ifndef XE_CFG_ASSERT
#include <assert.h>
#define XE_CFG_ASSERT(COND) assert((COND))
#endif

static inline void
xe_assert(bool condition)
{
    XE_CFG_ASSERT(condition);
}

enum {
    XE_OK = 0,
    XE_ERR = -65, // XE_ERR and lower values are considered errors
    /* GENERIC DEFINITIONS (-64...-1, negative values to be able to use them as invalid indices or any other positive data) */
    XE_NOOP,
    XE_NULL,
    XE_DEFAULT,
    XE_DELETED,
    XE_INVALID,
    /* GENERIC ERRORS (-99...-66) */
    XE_ERR_FATAL = -99, // Unrecoverable error.
    XE_ERR_ARG,
    XE_ERR_CONFIG,
    XE_ERR_UNINIT, // Required module not initialized.
    XE_ERR_UNKNOWN,
    /* PLATFORM ERRORS (-199...-99) */
    XE_ERR_PLAT = -199,
    XE_ERR_PLAT_FILE,
    XE_ERR_PLAT_MEM,
    /* HANDLE ERRORS (-299...-200) */
    XE_ERR_HND = -299,
    XE_ERR_HND_DANGLING,
};

#if defined(XE_CFG_LOG_DISABLED)
#define _XE_LOG(LL, ...) (void)
#elif defined(XE_CFG_LOG_RESTRICTED)
#define XE_LOG_DEBUG(...) (void)
#define XE_LOG_VERBOSE(...) (void)
#define XE_LOG(...) (void)
#define XE_LOG_WARN(...) (void)
#elif defined(XE_CFG_LOG_VERBOSE)
#define XE_LOG_DEBUG(...) (void)
#elif !defined(XE_DEBUG)
#define XE_LOG_DEBUG(...) (void)
#define XE_LOG_VERBOSE(...) (void)
#endif

#ifndef _XE_LOG
#define _XE_LOG(TAG,...) xe_log_ex(TAG, __FILE__, __LINE__, __VA_ARGS__)
#endif /* _XE_LOG */

#ifndef XE_LOG_VERBOSE
#define XE_LOG_VERBOSE(...) _XE_LOG("XE_VERBOSE", __VA_ARGS__)
#endif /* XE_LOG_VERBOSE */

#ifndef XE_LOG
#define XE_LOG(...) _XE_LOG("XE_LOG", __VA_ARGS__)
#endif /* XE_LOG */

#ifndef XE_LOG_WARN
#define XE_LOG_WARN(...) _XE_LOG("XE_WARN", __VA_ARGS__)
#endif /* XE_LOG_WARN */

#define XE_LOG_ERR(...) _XE_LOG("XE_ERR", __VA_ARGS__)
#define XE_LOG_PANIC(...) _XE_LOG("XE_PANIC", __VA_ARGS__)

/**
 * Extended log, usually called from log macros.
 * @param tag Log header for emitter identifier and log level.
 * @param file Filename, usually from __FILE__ macro.
 * @param line File line of the log call, usually from __LINE__ macro.
 * @param var_args Text format and values.
 */
void xe_log_ex(const char *tag, const char *file, int line, ...);

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

    /* Inputs */
    float mouse_x;
    float mouse_y;
    bool close;

    /* Internal state */
    char window_title[1024];
    void *window;
    void *log_stream;
    bool initialized;

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

xe_platform *xe_platform_create(xe_platform_config *config);
float xe_platform_update(void);
void xe_platform_shutdown(void);

int64_t xe_file_mtime(const char *path);

#endif  /* __XE_PLATFORM_H__ */
