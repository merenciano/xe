#include <llulu/lu_time.h>
#include <time.h>

#if !(defined(_POSIX_TIMERS) && _POSIX_TIMERS >= 1)

#ifdef _WIN32
#include <windows.h>

// The code below is copied from mingw-64 clock.c implementation: https://github.com/mirror/mingw-w64/blob/dcd990ed423381cf35702df9495d44f1979ebe50/mingw-w64-libraries/winpthreads/src/clock.c#L139
// --------------------------------------------------------------
#define POW10_7                 10000000
#define POW10_9                 1000000000

/* Number of 100ns-seconds between the beginning of the Windows epoch
 * (Jan. 1, 1601) and the Unix epoch (Jan. 1, 1970)
 */
#define DELTA_EPOCH_IN_100NS    INT64_C(116444736000000000)

/**
 * Get the time of the specified clock clock_id and stores it in the struct
 * timespec pointed to by tp.
 * @param  clock_id The clock_id argument is the identifier of the particular
 *         clock on which to act. The following clocks are supported:
 * <pre>
 *     CLOCK_REALTIME  System-wide real-time clock. Setting this clock
 *                 requires appropriate privileges.
 *     CLOCK_MONOTONIC Clock that cannot be set and represents monotonic
 *                 time since some unspecified starting point.
 *     CLOCK_PROCESS_CPUTIME_ID High-resolution per-process timer from the CPU.
 *     CLOCK_THREAD_CPUTIME_ID  Thread-specific CPU-time clock.
 * </pre>
 * @param  tp The pointer to a timespec structure to receive the time.
 * @return If the function succeeds, the return value is 0.
 *         If the function fails, the return value is -1,
 *         with errno set to indicate the error.
 */
int clock_gettime(int clock_id, struct timespec *tp)
{
    unsigned __int64 t;
    LARGE_INTEGER pf, pc;
    union {
        unsigned __int64 u64;
        FILETIME ft;
    }  ct, et, kt, ut;

    switch(clock_id) {
    case LU_TIME_CLOCK_REALTIME:
        {
#ifdef __TINYC__
            GetSystemTimeAsFileTime(&ct.ft);
#else
            GetSystemTimePreciseAsFileTime(&ct.ft);
#endif
            t = ct.u64 - DELTA_EPOCH_IN_100NS;
            tp->tv_sec = t / POW10_7;
            tp->tv_nsec = ((int) (t % POW10_7)) * 100;

            return 0;
        }

    case LU_TIME_CLOCK_MONOTONIC:
        {
            if (QueryPerformanceFrequency(&pf) == 0)
                return 0;

            if (QueryPerformanceCounter(&pc) == 0)
                return 0;

            tp->tv_sec = pc.QuadPart / pf.QuadPart;
            tp->tv_nsec = (int) (((pc.QuadPart % pf.QuadPart) * POW10_9 + (pf.QuadPart >> 1)) / pf.QuadPart);
            if (tp->tv_nsec >= POW10_9) {
                tp->tv_sec ++;
                tp->tv_nsec -= POW10_9;
            }

            return 0;
        }

    case LU_TIME_CLOCK_PROCESS:
        {
        if(0 == GetProcessTimes(GetCurrentProcess(), &ct.ft, &et.ft, &kt.ft, &ut.ft))
            return 0;
        t = kt.u64 + ut.u64;
        tp->tv_sec = t / POW10_7;
        tp->tv_nsec = ((int) (t % POW10_7)) * 100;

        return 0;
        }

    case LU_TIME_CLOCK_THREAD: 
        {
            if(0 == GetThreadTimes(GetCurrentThread(), &ct.ft, &et.ft, &kt.ft, &ut.ft))
                return 0;
            t = kt.u64 + ut.u64;
            tp->tv_sec = t / POW10_7;
            tp->tv_nsec = ((int) (t % POW10_7)) * 100;

            return 0;
        }

    default:
        break;
    }
    return 0;
}

#elif __STDC_VERSION__ >= 201112L

int clock_gettime(int clock_id, struct timespec *tp)
{
    // Always CLOCK_RUNTIME but better than nothing.
    return timespec_get(tp, TIME_UTC);
};

#else
#warning "Timer not implemented for this platform or version."
#endif
#endif

lu_timestamp lu_time_get_using_clock(int clock_id) {
    struct timespec time;
    clock_gettime(clock_id, &time);
    return time.tv_nsec + time.tv_sec * 1000000000;
}
