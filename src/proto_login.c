// lolercraft
// packet handler for login state
// client authentication handling
// encryption

#include "lolercraft/socket.h"
#include <lolercraft/encryption.h>
#include <lolercraft/logging.h>
#include <lolercraft/misctypes.h>
#include <lolercraft/protocol.h>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include <curl/curl.h>

// -- packet callbacks --
// uses identifiers listed by wiki.vg

// 0x0 login start
static bool sb_hello (s_client *client) {
    // get username
    char *username = p_read_string (client, NULL);
    if (!username) { return false; }

    strncpy (thread_name, username, sizeof (client->username));
    strncpy (client->username, username, sizeof (client->username));

    // get uuid
    uint64_t uuid[2];
    if (!p_read_nbytes (client, (uint8_t *) uuid, 16)) { return false; }
    client->uuid[0] = leflip_64 ((uint8_t *) &uuid[0]);
    client->uuid[1] = leflip_64 ((uint8_t *) &uuid[1]);

    log_debug ("login handshake received (user: %s, uuid: %llx%llx, token: %x)",
               client->username, client->uuid[0], client->uuid[1],
               *((uint32_t *) client->_token));

    // 0x1 - encryption request / hello packet
    RAND_bytes (client->_token_start, 4);

    buf_auto *packet = p_buf_init (0x1);

    if (!buf_addb (packet, 0)                             // 0 byte for string
        || !p_buf_str (packet, (char *) p_der, p_der_len) // public key
        || !p_buf_str (packet, (char *) client->_token_start,
                       4)                                // generated token
        || !buf_addb (packet, s_offline_mode ? 0 : 1)) { // bool for auth
        if (err_state)
            log_debug ("login handshake response error occured: %s", err_buf);
        return false;
    }

    return p_buf_send (client, packet);
}

// rsa decryption for login sequence
static bool _rsa (uint8_t *in, size_t in_len, uint8_t *out, size_t *out_len) {
    if (!in || !out || !out_len) return false;

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new (p_pkey, NULL);
    if (!ctx) {
        log_debug ("EVP_PKEY_CTX_new failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (EVP_PKEY_decrypt_init (ctx) <= 0
        || EVP_PKEY_CTX_set_rsa_padding (ctx, RSA_PKCS1_PADDING) <= 0
        || EVP_PKEY_decrypt (ctx, out, out_len, in, in_len) <= 0) {
        log_debug ("rsa decrypt failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        EVP_PKEY_CTX_free (ctx);
        return false;
    }

    EVP_PKEY_CTX_free (ctx);

    return true;
}

// initializing crypt contexts for client on login
static bool _init_crypt (s_client *client) {
    if (!client) return false;

    client->_encrypt = EVP_CIPHER_CTX_new ();
    client->_decrypt = EVP_CIPHER_CTX_new ();
    if (!client->_encrypt || !client->_decrypt) {
        log_debug ("client encryption init failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (!EVP_EncryptInit_ex (client->_encrypt, EVP_aes_128_cfb8 (), NULL,
                             client->_secret, client->_secret)
        || !EVP_DecryptInit_ex (client->_decrypt, EVP_aes_128_cfb8 (), NULL,
                                client->_secret, client->_secret)) {
        log_debug ("client encryption init failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return 0;
    }

    client->_encrypted = true;

    return true;
}

// 42 byte max output
static bool _mc_hash (s_client *client, char *out) {
    if (!client || !out) return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new ();
    if (!ctx) return false;

    uint8_t digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;

    if (EVP_DigestInit_ex (ctx, EVP_sha1 (), NULL) <= 0) {
        log_debug ("EVP_DigestInit_ex failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (EVP_DigestUpdate (ctx, client->_secret, client->_secret_len) <= 0
        || EVP_DigestUpdate (ctx, p_der, p_der_len) <= 0
        || EVP_DigestFinal_ex (ctx, digest, &digest_len) <= 0) {
        log_debug ("authentication hash digest failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        EVP_MD_CTX_free (ctx);
        return false;
    }

    EVP_MD_CTX_free (ctx);

    // tailor to non-standard mojang version
    int neg = (digest[0] & 0x80) != 0;

    if (neg) {
        uint8_t carry = 1;
        for (int i = (int) digest_len - 1; i >= 0; i--) {
            uint16_t val = (uint16_t) (~digest[i]) + carry;
            digest[i]    = (uint8_t) (val & 0xFF);
            carry        = (uint8_t) (val >> 8);
        }
    }

    // format into string
    char hex_tmp[65] = { 0 };
    char *ptr        = hex_tmp;
    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf (ptr, "%02x", digest[i]);
        ptr += 2;
    }

    char *start = hex_tmp;
    while (*start == '0' && *(start + 1) != '\0') { start++; }

    // finalize string
    if (neg) {
        sprintf (out, "-%s", start); // prefixed with -
    } else {
        strcpy (out, start);
    }

    return true;
}

struct _curl_resp_buf {
    char *buf;
    size_t len;
};

static size_t _curl_write (void *data, size_t size, size_t nmemb, void *userp) {
    size_t len                  = size * nmemb;
    struct _curl_resp_buf *resp = (struct _curl_resp_buf *) userp;

    char *buf = malloc (resp->len + len + 1);
    if (!buf) { return 0; }

    resp->buf = buf;
    memcpy (&(resp->buf[resp->len]), data, len);
    resp->len += len;
    resp->buf[resp->len] = 0; // null term

    return len;
}

static bool _auth (s_client *client) {
    char hash[42] = { 0 };
    if (!_mc_hash (client, hash)) return false;

    CURL *curl = curl_easy_init ();
    if (!curl) {
        log_debug ("authentication routine failure: curl_easy_init failed");
        return false;
    }

    // curl response buf & data
    struct _curl_resp_buf c_buf = { 0 };
    char *c_username            = NULL;
    char *c_hash                = NULL;

    // get needed strings for url building
    if (!(c_username = curl_easy_escape (curl, client->username, 0))
        || !(c_hash = curl_easy_escape (curl, hash, 0))) {
        log_debug ("authentication routine failure: curl_easy_init failed");
        if (c_username) curl_free (c_username);
        if (c_hash) curl_free (c_hash);
        curl_easy_cleanup (curl);
        return false;
    }

    // server request url
    // note: extra ?ip=<client> can be inserted to disable proxies (later)
    char req_url[1024];
    snprintf (req_url, sizeof (req_url),
              "https://sessionserver.mojang.com/session/minecraft/"
              "hasJoined?username=%s&serverId=%s",
              c_username, c_hash);

    curl_free (c_username);
    curl_free (c_hash);

    // disable cache
    struct curl_slist *headers = NULL;
    headers = curl_slist_append (headers, "Cache-Control: no-cache");
    headers = curl_slist_append (headers, "Pragma: no-cache");
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, headers);

    // configure for sending req
    curl_easy_setopt (curl, CURLOPT_URL, req_url);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, &_curl_write);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &c_buf);
    curl_easy_setopt (curl, CURLOPT_TIMEOUT, 5L); // 5s timeout
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "Minecraft-Server-C/1.0");

    CURLcode curl_err = curl_easy_perform (curl);
    if (curl_err) {
        // disconnect client
        buf_auto *packet = p_buf_init (0);
        p_buf_str (packet,
                   "{\"text\":\"unable to connect to authentication api\"}", 0);
        p_buf_send (client, packet);
        log_debug ("authentication api request failure: %s",
                   curl_easy_strerror (curl_err));
        free (c_buf.buf);
        curl_easy_cleanup (curl);
        return false;
    }

    long code    = 0;
    bool success = false;
    curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &code);

    if (code == 200) {
        // pull + verify data
        log_debug ("authentication success");
        success = true;
    } else {
        // disconnect client
        buf_auto *packet = p_buf_init (0);
        p_buf_str (packet,
                   "{\"text\":\"failed to authenticate with mojang servers\"}",
                   0);
        p_buf_send (client, packet);

        log_debug ("authentication failed: mojang api returned code %ld", code);
    }

    free (c_buf.buf);
    curl_easy_cleanup (curl);

    return success;
}

// 0x1 encryption response
static bool sb_key (s_client *client) {
    if (!client) return false;

    // encrypted client secret and token
    uint8_t *encrypted, *token;
    size_t encrypted_len, token_len;

    if (!p_read_array (client, &encrypted, &encrypted_len)) {
        if (err_state)
            log_debug ("login encryption error occured: %s", err_buf);
        return false;
    }

    if (!p_read_array (client, &token, &token_len)) {
        if (err_state)
            log_debug ("login encryption error occured: %s", err_buf);
        return false;
    }

    client->_secret_len = sizeof (client->_secret);
    client->_token_len  = sizeof (client->_token);

    // decrypt secret
    if (!_rsa (encrypted, encrypted_len, client->_secret, &client->_secret_len))
        return false;

    // decrypt token
    if (!_rsa (token, token_len, client->_token, &client->_token_len))
        return false;

    // token verification should happen here !! to add

    if (!_init_crypt (client)) return false;

    // authentication routine
    if (!s_offline_mode && !_auth (client)) return false;

    // THIS WOULD BE THE PLACE TO ENABLE COMPRESSION, DO LATER!

    // clientbound login success packet
    buf_auto *packet = p_buf_init (2);

    // form client uuid (be)
    uint8_t out_uuid[16];
    memcpy (&out_uuid[0], beflip_64 (client->uuid[0]), 8);
    memcpy (&out_uuid[8], beflip_64 (client->uuid[1]), 8);

    buf_append (packet, (uint8_t *) out_uuid, 16);
    p_buf_str (packet, client->username, 0);
    buf_addb (packet, 1);
    p_buf_send (client, packet);

    return true;
}

// 0x2 (not implemented yet)
static bool sb_custom_query_answer (s_client *client) { return true; }

// 0x3 change state to connection
static bool sb_login_acknowledged (s_client *client) {
    client->state = 4;
    return true;
}

// 0x4 (not implemented yet)
static bool sb_cookie_response (s_client *client) { return true; }

p_sb_cb p_login_sb[] = { &sb_hello, &sb_key, &sb_custom_query_answer,
                         &sb_login_acknowledged, &sb_cookie_response };
