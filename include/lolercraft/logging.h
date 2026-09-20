// lolercraft
// logging and error handling features

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// state will be non-zero if error
extern thread_local uint8_t err_state;
// string error buf from most-recent error (fixed 4196B)
extern thread_local char err_buf[4196];

// current thread name
extern thread_local char thread_name[256];

// logging mutex automatically used by log... functions
extern pthread_mutex_t log_mutex;

// enable/disable debug log printing
extern bool log_debug;

// out: true = print to stderr, false = only to err_buf
#define log_err(out, fmt, ...)                                           \
    {                                                                    \
        snprintf (err_buf, 4196, fmt, ##__VA_ARGS__);                    \
        err_state = 1;                                                   \
        if (out) {                                                       \
            pthread_mutex_lock (&log_mutex);                             \
            fprintf (stderr, "[%s] ERROR: %s \n", thread_name, err_buf); \
            pthread_mutex_unlock (&log_mutex);                           \
        }                                                                \
    }

#define log_malloc_err(size)                                      \
    log_err (true,                                                \
             "memory allocation failure (called at "__FILE_NAME__ \
             ":%d, func: %s, size: %lu)",                         \
             __LINE__, __FUNCTION__, (unsigned long) size);

#define log_debug(fmt, ...)                                        \
    {                                                              \
        if (log_debug) {                                           \
            pthread_mutex_lock (&log_mutex);                       \
            fprintf (stdout, "[%s] DEBUG: " fmt "\n", thread_name, \
                     ##__VA_ARGS__);                               \
            pthread_mutex_unlock (&log_mutex);                     \
        }                                                          \
    }

#define log(fmt, ...)                                                   \
    {                                                                   \
        pthread_mutex_lock (&log_mutex);                                \
        fprintf (stdout, "[%s] " fmt "\n", thread_name, ##__VA_ARGS__); \
        pthread_mutex_unlock (&log_mutex);                              \
    }

#ifdef __cplusplus
}
#endif
