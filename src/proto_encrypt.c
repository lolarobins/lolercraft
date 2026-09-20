#include <lolercraft/misctypes.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include <lolercraft/logging.h>
#include <lolercraft/protocol.h>

EVP_PKEY *p_pkey;
uint8_t *p_der;
int p_der_len;

// initialize packet encryption keys
bool p_encrypt_init () {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id (EVP_PKEY_RSA, NULL);
    if (EVP_PKEY_keygen_init (ctx) <= 0
        || EVP_PKEY_CTX_set_rsa_keygen_bits (ctx, 1024) <= 0
        || (EVP_PKEY_keygen (ctx, &p_pkey) <= 0)) {
        log_err (true, "key generation failed: %s",
                 ERR_reason_error_string (ERR_peek_last_error ()));
        EVP_PKEY_CTX_free (ctx);
        return false;
    }
    EVP_PKEY_CTX_free (ctx);

    if ((p_der_len = i2d_PUBKEY (p_pkey, NULL)) <= 0) {
        log_err (true, "key generation failed: %s",
                 ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (!(p_der = malloc (p_der_len))) {
        log_malloc_err (p_der_len);
        return false;
    }

    uint8_t *pub_tmp = p_der;

    if (i2d_PUBKEY (p_pkey, &pub_tmp) <= 0) {
        log_err (true, "key generation failed: %s",
                 ERR_reason_error_string (ERR_peek_last_error ()));
        free (p_der);
        return false;
    };

    return true;
}

void p_encrypt_free () {
    EVP_PKEY_free (p_pkey);
    free (p_der);
}

void p_client_free (s_client *client) {
    if (client->_encrypt)  EVP_CIPHER_CTX_cleanup(client->_encrypt);
    if (client->_decrypt) EVP_CIPHER_CTX_cleanup(client->_decrypt);
}

bool p_encrypt (s_client *client, buf_auto *buf) {
    if (!client || !buf) return false;

    // do nothing :p
    if (!client->_encrypted) { return true; }

    int new_len;
    if (EVP_EncryptUpdate (client->_encrypt, buf->buf, &new_len, buf->buf,
                           buf->len)
        <= 0) {
        log_debug ("decryption update failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (new_len != buf->len) {
        log_debug ("encryption failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    return true;
}

bool p_decrypt (s_client *client, uint8_t *buf, size_t buf_len) {
    if (!client || !buf) return false;

    if (!client->_encrypted) { return true; }

    int new_len;
    if (EVP_DecryptUpdate (client->_decrypt, buf, &new_len, buf,
                           buf_len)
        <= 0) {
        log_debug ("decryption update failed: %s",
                   ERR_reason_error_string (ERR_peek_last_error ()));
        return false;
    }

    if (new_len != buf_len) {
        log_debug ("decryption length invalid (wanted %lu, got %d)", buf_len,
                   new_len);
        return false;
    }

    return true;
}
