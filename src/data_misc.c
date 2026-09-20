// lolercraft
// utils for converting datatypes

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <lolercraft/logging.h>
#include <lolercraft/types.h>
#include <string.h>

// -- auto expanding bufs/strings --

#define BK_SIZE 8192

// get allocated mem len needed for a given size
static inline size_t mem_needed (size_t len) {
    return ((len / BK_SIZE) + 1) * BK_SIZE;
}

// add new memory to str
static inline bool mem_resize_s (str_auto *str, size_t newlen) {
    size_t needed = mem_needed (newlen);

    if (needed != str->_alen) {
        void *old_ptr = str->str;
        if (!(str->str = realloc (str->str, needed))) {
            free (old_ptr);
            log_malloc_err (needed);
            return false;
        }
    }

    return true;
}

// same as above for bufs
static inline bool mem_resize_b (buf_auto *buf, size_t newlen) {
    size_t needed = mem_needed (newlen);
    if (needed != buf->_alen) {
        void *old_ptr = buf->buf;
        if ((buf->buf = realloc (buf->buf, needed))) {
            free (old_ptr);
            log_malloc_err (needed);
            return false;
        }
    }

    return true;
}

str_auto *str_init (char *text) {
    str_auto *str = malloc (sizeof (str_auto));
    if (!str) {
        log_malloc_err (sizeof (str_auto));
        return NULL;
    }

    size_t str_len = text ? strlen (text) : 0;
    size_t alen    = mem_needed (str_len + 1);

    str->str = malloc (alen);
    if (!str->str) {
        log_malloc_err (alen);
        return NULL;
    }

    if (text) strcpy (str->str, text);
    else
        str->str[0] = '\0';

    str->len   = str_len;
    str->_alen = alen;
    return str;
}

void *str_free (str_auto *str) {
    if (str->str) free (str->str);
    if (str) free (str);
    return NULL;
}

char *str_detach (str_auto *str, size_t *len) {
    if (!str) return NULL;

    if (len) *len = str->len;
    char *buf = str->str;
    free (str);
    return buf;
}

bool str_clear (str_auto *str) {
    if (!str) return false;

    str->str[0] = '\0';
    str->len    = 0;
    return mem_resize_s (str, 0);
}

bool str_printf (str_auto *str, const char *fmt, ...) {
    if (!str || !fmt) return false;

    va_list args;

    va_start (args, fmt);
    size_t len = vsnprintf (NULL, 0, fmt, args);
    va_end (args);

    if (!mem_resize_s (str, str->len + len + 1)) return false;

    va_start (args, fmt);
    vsnprintf (&str->str[str->len], len + 1, fmt, args);
    str->len += len;
    va_end (args);

    return true;
}

bool str_append (str_auto *str, const char *new) {
    if (!str || !new) return false;

    size_t new_len = strlen (new);
    if (!mem_resize_s (str, str->len + new_len + 1)) return false;
    strcpy (&str->str[str->len], new);
    str->len += new_len;
    return true;
}

bool str_add_char (str_auto *str, char c) {
    if (!str) return false;

    if (!mem_resize_s (str, str->len + 2)) return false;

    str->str[str->len++] = c;
    str->str[str->len]   = '\0';

    return true;
}

buf_auto *buf_init () {
    buf_auto *buf = malloc (sizeof (buf_auto));
    if (!buf) {
        log_malloc_err (sizeof (buf_auto));
        return NULL;
    }

    size_t alen = mem_needed (0);

    buf->buf = malloc (alen);
    if (!buf->buf) {
        log_malloc_err (alen);
        return NULL;
    }

    buf->len   = 0;
    buf->_alen = alen;
    return buf;
}

void *buf_free (buf_auto *buf) {
    if (buf->buf) free (buf->buf);
    if (buf) free (buf);
    return NULL;
}

uint8_t *buf_detach (buf_auto *buf, size_t *len) {
    if (!buf) return NULL;

    if (len) *len = buf->len;
    uint8_t *ret_val = buf->buf;
    free (buf);
    return ret_val;
}

bool buf_clear (buf_auto *buf) {
    if (!buf) return false;

    buf->len = 0;
    return mem_resize_b (buf, 0);
}

bool buf_expand (buf_auto *buf, size_t len) {
    if (!buf) return false;
    if (!len) return true;

    return mem_resize_b (buf, buf->len + len);
}

bool buf_printf (buf_auto *buf, const char *fmt, ...) {
    if (!buf || !fmt) return false;

    va_list args;

    va_start (args, fmt);
    size_t len = vsnprintf (NULL, 0, fmt, args);
    va_end (args);

    if (!mem_resize_b (buf, buf->len + len + 1)) return false;

    va_start (args, fmt);
    vsnprintf ((char *) &buf->buf[buf->len], len + 1, fmt, args);
    buf->len += len;
    va_end (args);

    return true;
}

bool buf_append (buf_auto *buf, uint8_t *b, size_t len) {
    if (!buf || !b) return false;

    if (!mem_resize_b (buf, buf->len + len)) return false;
    memcpy (&buf->buf[buf->len], b, len);
    buf->len += len;
    return true;
}

bool buf_add_byte (buf_auto *buf, uint8_t b) {
    if (!buf) return false;

    if (!mem_resize_b (buf, buf->len + 1)) return false;
    buf->buf[buf->len++] = b;

    return true;
}

// -- varint data type handling --

static int64_t _vint_decode (const uint8_t *buf, size_t len, size_t *nbytes,
                             uint8_t bmax) {
    if (!buf || bmax > 10) return 0;

    err_state        = 0;
    uint64_t ret_val = 0;

    for (size_t i = 0, shift = 0; i < len;) {
        uint8_t byte = buf[i];

        ret_val |= (uint64_t) (byte & 0x7F) << shift;
        i++;

        if ((byte & 0x80) == 0) {
            if (nbytes) *nbytes = i;
            return (int64_t) ret_val;
        }

        shift += 7;

        if (i >= bmax) {
            if (nbytes) *nbytes = 0;
            log_err (false, "invalid vint");
            return 0;
        }
    }

    if (nbytes) *nbytes = 0;
    log_err (false, "invalid vint");
    return 0;
}

int32_t vint_decode32 (const uint8_t *buf, size_t len, size_t *nbytes) {
    return _vint_decode (buf, len, nbytes, 5);
}

int64_t vint_decode64 (const uint8_t *buf, size_t len, size_t *nbytes) {
    return _vint_decode (buf, len, nbytes, 10);
}

vint vint_encode32 (int32_t val) {
    vint ret_val  = { 0 };
    uint32_t uval = (uint32_t) val;
    size_t i      = 0;

    do {
        uint8_t byte = (uint8_t) (uval & 0x7F);
        uval >>= 7;

        if (uval != 0) { byte |= 0x80; }

        ret_val.buf[i++] = byte;
    } while (uval != 0 && i < 5);

    if (uval != 0) return (vint) { 0 };

    ret_val.len = (uint8_t) i;
    return ret_val;
}

vint vint_encode64 (int64_t val) {
    vint ret_val  = { 0 };
    uint64_t uval = (uint64_t) val;
    size_t i      = 0;

    do {
        uint8_t byte = (uint8_t) (uval & 0x7F);
        uval >>= 7;

        if (uval != 0) { byte |= 0x80; }

        ret_val.buf[i++] = byte;
    } while (uval != 0 && i < 10);

    if (uval != 0) return (vint) { 0 };

    ret_val.len = (uint8_t) i;
    return ret_val;
}

// -- endian flipping --

uint8_t *beflip_16 (uint16_t n) {
    static thread_local uint8_t buf[2];
    uint16_t flip = __builtin_bswap16 (n);
    memcpy (buf, &flip, 2);
    return buf;
}

uint8_t *beflip_32 (uint32_t n) {
    static thread_local uint8_t buf[4];
    uint32_t flip = __builtin_bswap32 (n);
    memcpy (buf, &flip, 4);
    return buf;
}

uint8_t *beflip_64 (uint64_t n) {
    static thread_local uint8_t buf[8];
    uint64_t flip = __builtin_bswap64 (n);
    memcpy (buf, &flip, 8);
    return buf;
}

uint16_t leflip_16 (uint8_t buf[2]) {
    uint16_t flip;
    memcpy (&flip, buf, 2);
    flip = __builtin_bswap16 (flip);
    return flip;
}

uint32_t leflip_32 (uint8_t buf[4]) {
    uint32_t flip;
    memcpy (&flip, buf, 4);
    flip = __builtin_bswap32 (flip);
    return flip;
}

uint64_t leflip_64 (uint8_t buf[8]) {
    uint64_t flip;
    memcpy (&flip, buf, 8);
    flip = __builtin_bswap64 (flip);
    return flip;
}
