// lolercraft
// client packet handling

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "types.h"
#include "socket.h"

// -- packet handling --

// receive next packet
extern int32_t p_receive (s_client *client);
extern bool p_decrypt (s_client *client, uint8_t *buf, size_t buf_len);

// -- data reading --

// read an unsigned 8 bit int
extern uint8_t p_read_uint8 (s_client *client);

// read an unsigned short (u16)
extern uint16_t p_read_uint16 (s_client *client);

// read a varint
extern int32_t p_read_vint32 (s_client *client);
extern int64_t p_read_vint64 (s_client *client);

// read nbytes into a buf
extern bool p_read_nbytes (s_client *client, uint8_t *buf, size_t len);

// read and allocate a string
extern char *p_read_string (s_client *client, size_t *len);

// read and allocate a byte array
// output can be null to skip instead of store
extern bool p_read_array (s_client *client, uint8_t **output, size_t *len);

// -- data sending --

extern buf_auto *p_buf_init (int32_t id);

extern bool p_buf_str (buf_auto *packet, char *str, size_t len);

extern bool p_encrypt (s_client *client, buf_auto *buf);
extern bool p_buf_send (s_client *client, buf_auto *buf);

extern void p_client_free (s_client *client);

// -- packet struct --

// serverbound packet callback
typedef bool (*p_sb_cb) (s_client *client);

// -- packets: login state --

// -- packets: configuration state --

// -- packets: play state --

#ifdef __cplusplus
}
#endif
