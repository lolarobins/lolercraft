// lolercraft
// client packet handling

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <lolercraft/logging.h>
#include <lolercraft/protocol.h>

// -- packet receiving --

// return packet id
int32_t p_receive (s_client *client) {
    if (!client) { return -1; }

    err_state = 0;

    // -- setup/modify packet buffer --
    if (!client->_p_buf) {
        if (!(client->_p_buf = malloc (S_MIN_PACKET_BUF))) {
            log_malloc_err (S_MIN_PACKET_BUF);
            return -1;
        }

        client->_p_buf_len = 0;
    } else if (client->_p_buf_len > S_MIN_PACKET_BUF) {
        void *old_ptr = client->_p_buf;

        if (!(client->_p_buf = realloc (old_ptr, S_MIN_PACKET_BUF))) {
            log_malloc_err (S_MIN_PACKET_BUF);
            free (old_ptr);
            return -1;
        }
    }

    // -- get length --
    uint8_t len_buf[5] = { 0 };
    size_t nbytes;

    for (size_t i = 0;; i++) {
        if (i > 4) {
            log_err (false, "invalid packet length value");
            return -1;
        }

        int read_ret = read (client->_fd, &len_buf[i], 1);
        if (read_ret <= 0) {
            if (errno) {
                log_err (false, "length read failed: %s", strerror (errno));
            } else {
                log_err (false, "received %dB", read_ret);
            }
            return -1;
        }

        // decrypt this now
        if (!p_decrypt (client, &len_buf[i], 1)) return -1;

        if ((len_buf[i] & 0x80) == 0) break;
    }

    client->_p_buf_len = vint_decode32 (len_buf, 5, &nbytes);
    if (!nbytes) {
        log_err (false, "invalid packet length value");
        return -1;
    }

    client->_p_buf_cur = 0;

    // -- read packet --
    if (client->_p_buf_len > S_MIN_PACKET_BUF) {
        void *old_ptr = client->_p_buf;

        if (!(client->_p_buf = realloc (old_ptr, client->_p_buf_len))) {
            log_malloc_err (client->_p_buf_len);
            free (old_ptr);
            return -1;
        }
    }

    for (size_t i = 0; i < client->_p_buf_len;) {
        int read_ret
           = read (client->_fd, client->_p_buf + i, client->_p_buf_len - i);
        if (read_ret <= 0) {
            if (read_ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                continue;

            if (errno) {
                log_err (false, "packet data read failed: %s",
                         strerror (errno));
            } else {
                log_err (false, "client disconnected during packet read");
            }
            return -1;
        }
        i += read_ret;
    }

    // decrypt
    if (!p_decrypt (client, client->_p_buf, client->_p_buf_len)) return -1;

    // get packet id
    int32_t packet = vint_decode32 (client->_p_buf, client->_p_buf_len,
                                    &client->_p_buf_cur);
    if (!client->_p_buf_cur) {
        log_err (false, "invalid packet id value");
        return -1;
    }

    return packet;
}

// -- data reading --

uint8_t p_read_uint8 (s_client *client) {
    if (!client) {return 0;}
    if (1 > client->_p_buf_len - client->_p_buf_cur) {
        log_err (false, "p_read_uint8: not enough data remaining in packet");
        return 0;
    }

    uint8_t ret_val = *(client->_p_buf + client->_p_buf_cur);
    client->_p_buf_cur++;

    return ret_val;
}

uint16_t p_read_uint16 (s_client *client) {
    if (!client) { return 0; }

    // bounds check
    if (2 > client->_p_buf_len - client->_p_buf_cur) {
        log_err (false, "p_read_uint16: not enough data remaining in packet");
        return 0;
    }

    uint16_t ret_val = leflip_16 (client->_p_buf + client->_p_buf_cur);
    client->_p_buf_cur += 2;

    return ret_val;
}

int32_t p_read_vint32 (s_client *client) {
    if (!client) { return 0; }

    size_t nbytes;
    int32_t ret_val
       = vint_decode32 (client->_p_buf + client->_p_buf_cur,
                        client->_p_buf_len - client->_p_buf_cur, &nbytes);

    client->_p_buf_cur += nbytes;
    return ret_val;
}

int64_t p_read_vint64 (s_client *client) {
    if (!client) { return 0; }

    size_t nbytes;
    int32_t ret_val
       = vint_decode64 (client->_p_buf + client->_p_buf_cur,
                        client->_p_buf_len - client->_p_buf_cur, &nbytes);

    client->_p_buf_cur += nbytes;
    return ret_val;
}

bool p_read_nbytes (s_client *client, uint8_t *buf, size_t len) {
    if (!client) { return false; }

    if (len > client->_p_buf_len - client->_p_buf_cur) {
        log_err (false,
                 "p_read_nbytes: not enough data remaining in packet "
                 "(requested: %luB, remaining: %luB)",
                 len, client->_p_buf_len - client->_p_buf_cur);
        return false;
    }

    if (buf) memcpy (buf, client->_p_buf + client->_p_buf_cur, len);

    client->_p_buf_cur += len;

    return true;
}

// will return true but NOT allocate a buffer when len = 0
bool p_read_array (s_client *client, uint8_t **output, size_t *len) {
    if (!client) { return false; }

    int32_t size = p_read_vint32 (client);

    if (output) {
        *output = malloc (size);
        if (!*output) {
            log_malloc_err (size);
            return false;
        }
    }

    if (!p_read_nbytes (client, output ? *output : NULL, size)) {
        if (output) free (*output);
        return false;
    }

    if (len) *len = size;

    return true;
}

char *p_read_string (s_client *client, size_t *len) {
    int32_t str_len = p_read_vint32 (client);
    if (err_state) {
        log_err (false, "p_read_string: invalid string size") return NULL;
    }

    char *ret_val = malloc (str_len + 1);
    if (!ret_val) { log_malloc_err (str_len + 1); }
    ret_val[str_len] = 0;

    // blank string permitted
    if (!str_len) {
        strcpy (ret_val, "");
        if (len) *len = 0;
        return ret_val;
    }

    memcpy (ret_val, client->_p_buf + client->_p_buf_cur, str_len);
    client->_p_buf_cur += str_len;
    if (len) *len = str_len;
    return ret_val;
}

// -- packet sending --

buf_auto *p_buf_init (int32_t id) {
    buf_auto *buf = buf_init ();
    if (!buf) return NULL;

    vint packet_id = vint_encode32 (id);
    if (!buf_append (buf, packet_id.buf, packet_id.len)) return NULL;

    return buf;
}

bool p_buf_str (buf_auto *packet, char *str, size_t len) {
    if (!str) return false;
    if (!len) len = strlen (str);

    vint str_len = vint_encode32 (len);
    return buf_append (packet, str_len.buf, str_len.len)
           && buf_append (packet, (uint8_t *) str, len);
}

bool p_buf_send (s_client *client, buf_auto *buf) {
    vint packet_len = vint_encode32 (buf->len);

    buf_auto *out_buf = buf_init ();
    if (!buf_append (out_buf, packet_len.buf, packet_len.len)
        || !buf_append (out_buf, buf->buf, buf->len)
        || !p_encrypt (client, out_buf)) {
        buf_free (buf);
        buf_free (out_buf);
        return false;
    }
    buf_free (buf);

    if (write (client->_fd, out_buf->buf, out_buf->len) <= 0) {
        if (errno)
            log_err (false, "write error (%luB): %s", buf->len,
                     strerror (errno));
        buf_free (out_buf);
        return false;
    }
    buf_free (out_buf);

    return true;
}
