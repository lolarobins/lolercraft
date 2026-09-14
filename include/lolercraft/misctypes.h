// lolercraft
// utils for converting datatypes

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

// -- auto expanding bufs/strings --
// currently, block sizes for buffers/strings is hardcoded at 8192B in types.c

typedef struct buf_auto {
    uint8_t *buf;
    size_t len, _alen;
} buf_auto;

typedef struct str_auto {
    char *str;
    size_t len, _alen;
} str_auto;

/// init a new auto string
/// @param text optional: text to start with
/// @return allocated auto string struct, must be freed with either str_free or
/// str_detach
extern str_auto *str_init (char *text);

/// free an auto string
/// @param str auto string struct
/// @return null pointer (makes setting ptr to null nicer)
extern void *str_free (str_auto *str);

/// keep string buf, detach and free str_auto struct
/// @param str auto string struct
/// @param len optional: storage for string length
/// @return created string, can be freed with standard free call
extern char *str_detach (str_auto *str, size_t *len);

/// clear the contents of an auto string
/// @param str auto string struct
/// @return false if err occured
extern bool str_clear (str_auto *str);

/// append to a string with printf features
/// @param str auto string struct
/// @param fmt printf format
/// @return false if err occured
extern bool str_printf (str_auto *str, const char *fmt, ...);

/// append to a string (faster than str_printf)
/// @param str auto string struct
/// @param new new string to append
/// @return false if err occured
extern bool str_append (str_auto *str, const char *new);

/// append a character to a string
/// @param str auto string struct
/// @param c character to append
/// @return false if err occured
extern bool str_putchar (str_auto *str, char c);

extern buf_auto *buf_init ();
extern void *buf_free (buf_auto *buf);
extern uint8_t *buf_detach (buf_auto *buf, size_t *len);
extern bool buf_clear (buf_auto *buf);
extern bool buf_expand (buf_auto *buf, size_t len);
extern bool buf_printf (buf_auto *buf, const char *fmt, ...);
extern bool buf_append (buf_auto *buf, uint8_t *b, size_t len);
extern bool buf_addb (buf_auto *buf, uint8_t b);

// -- varints specifically applicable to the mc proto --

typedef struct vint {
    uint8_t len, buf[10];
} vint;

/// decode a 32 bit varint from a buffer
/// @param buf input buffer
/// @param len input buffer max length
/// @param nbytes optional: outputs total bytes read
/// @return int value of varint
extern int32_t vint_decode32 (const uint8_t *buf, size_t len, size_t *nbytes);

/// decode a 64 bit varint from a buffer
/// @param buf input buffer
/// @param len input buffer max length
/// @param nbytes optional: outputs total bytes read
/// @return int value of varint
extern int64_t vint_decode64 (const uint8_t *buf, size_t len, size_t *nbytes);

extern vint vint_encode32 (int32_t val);
extern vint vint_encode64 (int64_t val);

// -- endian flipping --
// assumes little endian host (later be support can happen!)

/// flip a 16 bit int to big endian (output)
/// @param n number to flip
/// @return thread-safe static buffer with flipped value
extern uint8_t *beflip_16 (uint16_t n);

/// flip a 32 bit int to big endian (output)
/// @param n number to flip
/// @return thread-safe static buffer with flipped value
extern uint8_t *beflip_32 (uint32_t n);

/// flip a 64 bit int to big endian (output)
/// @param n number to flip
/// @return thread-safe static buffer with flipped value
extern uint8_t *beflip_64 (uint64_t n);

/// flip a 16 bit int buf to low endian (input)
/// @param buf buffer to read from
/// @return resulting int
extern uint16_t leflip_16 (uint8_t buf[2]);

/// flip a 32 bit int buf to low endian (input)
/// @param buf buffer to read from
/// @return resulting int
extern uint32_t leflip_32 (uint8_t buf[4]);

/// flip a 64 bit int buf to low endian (input)
/// @param buf buffer to read from
/// @return resulting int
extern uint64_t leflip_64 (uint8_t buf[8]);

#ifdef __cplusplus
}
#endif
