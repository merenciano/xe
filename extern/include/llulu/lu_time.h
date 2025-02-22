#ifndef __LLULU_TIME_H__
#define __LLULU_TIME_H__

#include <stdint.h>

/** Compile-time configuration option to specify the default clock. */
#define LU_TIME_DEFAULT_CLOCK LU_TIME_CLOCK_MONOTONIC

/**
 * @brief Clock definitions.
 * @note The internal values of this enumeration should be used as 'clock_id'.
 */
enum lu_time_clock_e {
    /** System-wide clock that measures real time. It can be manually setted
     *  with appropiate privileges. */
    LU_TIME_CLOCK_REALTIME,

    /** Not affected by changes in the system clock but affected by frequency
     *  adjustments. */
    LU_TIME_CLOCK_MONOTONIC,

    /** Masures CPU time consumed by all the threads in the process. */
    LU_TIME_CLOCK_PROCESS,

    /** Measures CPU time consumed by this thread. */
    LU_TIME_CLOCK_THREAD
};

/** A specific point in time in nanoseconds. */
typedef int64_t lu_timestamp;

/**
 * @brief Gets the current timestamp in nanoseconds.
 * The origin of the nanosecond counter is not specified, but any measurement
 * taken from timestamps obtained during program execution is guaranteed to be
 * accurate.
 * @param clock_id Numeric identifier of the desired clock to use, @ref enum
 * lu_time_clock_e.
 * @return Current timestamp in nanoseconds.
 */
lu_timestamp lu_time_get_using_clock(int clock_id);

/**
 * @brief Gets the current timestamp in nanoseconds.
 * @note This function calls @ref lu_time_get_using_clock with the default
 * clock, @see LU_TIME_DEFAULT_CLOCK.
 * @return Current timestamp in nanoseconds.
 */
static inline lu_timestamp lu_time_get(void) {
    return lu_time_get_using_clock(LU_TIME_DEFAULT_CLOCK);
}

/**
 * @brief Gets the duration (in nanoseconds) of the elapsed time between the
 * specified timestamp and the current time.
 * @param time_from Beginning of the time span.
 * @return Nanoseconds elapsed since the timestamp.
 */
static inline int64_t lu_time_elapsed(lu_timestamp time_from) {
    return lu_time_get() - time_from;
}

/** Time unit conversion helpers. */
static inline float lu_time_sec(lu_timestamp ns) { return ns / 1000000000.0f; }
static inline int64_t lu_time_ms(lu_timestamp ns) { return ns / 1000000; }

#endif /* __LLULU_TIME_H__ */
