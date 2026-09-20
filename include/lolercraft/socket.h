// lolercraft
// socket & client handling for both ipv4 and ipv6

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// (from main)
extern volatile bool running;

// 54 max len for ipv6 + port
#define S_ADDR_LEN_MAX 54

// min packet buf size reuse to avoid unnecessary mallocs
#define S_MIN_PACKET_BUF 16392

// client & player information struct
typedef struct s_client {
    // -- client connection information --
    char address[S_ADDR_LEN_MAX];
    uint8_t state; // 1 = status, 2/3 = join, 4 = config, 5 = play

    // -- player information --
    char username[17]; // username
    uint64_t uuid[2];  // uuid

    // -- player client configuration --
    char *brand;             // client brand
    char *locale;            // language setting
    uint8_t render_distance; // user render distance
    uint8_t chat_mode;       // 0 = enabled, 1 = commands, 2 = none
    bool chat_col;           // use chat colours?
    bool chat_swear_filter;  // profanity filter (mojang req.)
    uint8_t skin_parts;      // bitmask of skin parts showing
    uint8_t main_hand;       // 0 = left, 1 = right
    bool show_in_list;       // show in player list?
    uint8_t particle_status; // 0 = all, 1 = decreased, 2 = minimal

    // -- internal --
    int _proto; // proto version

    int _fd;         // communication fd
    bool _serv6;     // conn server is ipv6?
    bool _encrypted; // using encryption
    bool _zlib;      // using zlib compression?

    pthread_t _thread; // handling thread

    uint8_t *_p_buf;   // packet buf
    size_t _p_buf_len; // buf len
    size_t _p_buf_cur; // read cursor

    uint8_t _token[256], _token_start[4]; // verify token
    size_t _token_len;                    // verify token len set by rsa decrypt
    uint8_t _secret[256];                 // secret key
    size_t _secret_len;                   // secret len set by rsa decrypt

    char *_skin;      // base64 skin
    char *_signature; // player signature

    void *_encrypt, *_decrypt; // encryption and decryption contexts
} s_client;

// flags
#define S_FLAG_IPV4   0b1
#define S_FLAG_IPV6   0b10
#define S_FLAG_NOAUTH 0b100

// socket flag to only be set before starting server
extern uint8_t s_flags;

// string form of addresses for ipv4/ipv6
extern char s_addr4[256], s_addr6[256];

// ipv4/ipv6 file descriptors
extern int s_fd4, s_fd6;

// ipv4/ipv6 ports
extern uint16_t s_port4, s_port6;

// ipv4/ipv6 threads
extern pthread_t s_thread4, s_thread6;

// skip minecraft auth
extern bool s_offline_mode;

#define s_set_addr4(addr, port)       \
    {                                 \
        s_port4 = port;               \
        strlcpy (s_addr4, addr, 256); \
        s_flags |= S_FLAG_IPV4;       \
    }
#define s_set_addr6(addr, port)       \
    {                                 \
        s_port6 = port;               \
        strlcpy (s_addr6, addr, 256); \
        s_flags |= S_FLAG_IPV6;       \
    }

extern int s_start ();

extern void s_cleanup ();

// -- server status --

// internal use variables
extern pthread_mutex_t _st_mutex;
extern time_t _st_expire;
extern char *_st_string;
extern size_t _st_string_len;

// fetch server status json string
extern char *status_fetch ();
// update the status description
extern bool status_update_description (const char *desc);
// status cleanup (called already by s_cleanup)
extern void status_cleanup ();

#ifdef __cplusplus
}
#endif
