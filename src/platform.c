#include "platform.h"
#include "xe.h"

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

typedef struct xe_plat {
    xe_plat_desc desc;
    FILE *log_stream;
    FILE *debug_stream;
    bool initialized;
} xe_plat;

static xe_plat g_plat;

int
xe_plat_init(xe_plat_desc *desc)
{
    if (g_plat.initialized) {
        XE_LOG("xe_plat_init call ignored: platform already initialized.");
        return XE_NOOP;
    }

    g_plat.desc = *desc;
    if (!g_plat.desc.log_filename || *g_plat.desc.log_filename == '\0') {
        g_plat.desc.log_filename = g_null_stream;
    }
    g_plat.log_stream = fopen(g_plat.desc.log_filename, "w");
    if (!g_plat.log_stream) {
        XE_LOG_ERR("fopen file %s failed, using tmpfile instead.", g_plat.desc.log_filename);
        g_plat.log_stream = tmpfile();
        if (!g_plat.log_stream) {
            XE_LOG_ERR("tmpfile failed, using stdout instead.");
            g_plat.log_stream = stdout;
        }
    }
    xe_assert(g_plat.log_stream);

#ifdef XE_DEBUG
    g_plat.debug_stream = fopen("debug_logs.txt", "w");
    if (!g_plat.debug_stream) {
        XE_LOG_DEBUG("fopen file debug_logs.txt failed, using stdout instead.");
        g_plat.debug_stream = stdout;
    }
#endif /* XE_DEBUG */

    g_plat.initialized = true;
    return XE_OK;
}

void
xe_log_ex(const char *tag, const char *file, int line, ...)
{
    va_list args;
    va_start(args, line);
    const char *fmt = va_arg(args, const char*);
#ifdef XE_DEBUG
    fprintf(g_plat.debug_stream, "[%s] %s(%d): ", tag, file, line);
    va_list dbg_args;
    va_copy(dbg_args, args);
    vfprintf(g_plat.debug_stream, fmt, dbg_args);
    va_end(dbg_args);
    fputc('\n', g_plat.debug_stream);
#endif /* XE_DEBUG */
    fprintf(g_plat.log_stream, "[%s] %s(%d): ", tag, file, line);
    vfprintf(g_plat.log_stream, fmt, args);
    va_end(args);
    fputc('\n', g_plat.log_stream);
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
