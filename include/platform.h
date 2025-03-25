#ifndef __XE_PLATFORM_H__
#define __XE_PLATFORM_H__

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

#ifndef XE_LOG_DEBUG
#define XE_LOG_DEBUG(...) _XE_LOG("XE_DEBUG", __VA_ARGS__)
#endif /* XE_LOG_DEBUG */

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

typedef struct xe_plat_desc {
    const char *log_filename;
} xe_plat_desc;

int xe_plat_init(xe_plat_desc *desc);


/**
 * Extended log, usually called from log macros.
 * @param tag Log header for emitter identifier and log level.
 * @param file Filename, usually from __FILE__ macro.
 * @param line File line of the log call, usually from __LINE__ macro.
 * @param var_args Text format and values.
 */
void xe_log_ex(const char *tag, const char *file, int line, ...);
int64_t xe_file_mtime(const char *path);

#endif  /* __XE_PLATFORM_H__ */