#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/pem.h>

#include <lolercraft/misctypes.h>
#include <lolercraft/protocol.h>

EVP_PKEY *p_pkey;
uint8_t *p_der;
int p_der_len;

extern bool p_encrypt_init ();
extern void p_encrypt_free ();

#ifdef __cplusplus
}
#endif
