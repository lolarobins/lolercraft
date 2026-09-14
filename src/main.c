// lolercraft

#include "lolercraft/json.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/pem.h>

#include <lolercraft/encryption.h>
#include <lolercraft/logging.h>
#include <lolercraft/protocol.h>

volatile bool running = true;

thread_local uint8_t err_state;
thread_local char err_buf[4196];

thread_local char thread_name[256];

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

bool log_debug = true;

int main () {
    json_object *obj;
    if (!(obj = json_decode ("{\"meow\":true,\"gay\":null}", 24))) {
        log_err (true, "%s", err_buf);
        return -1;
    }

    size_t len;
    char *str = json_encode (obj, &len, true);
    if (!str) {
        log_err (true, "%s", err_buf);
        return -1;
    }
    log("%ld, %s", len, str);

    extern json_object st_root;
    str = json_encode (&st_root, &len, true);
    if (!str) {
        log_err (true, "%s", err_buf);
        return -1;
    }
    log("%ld, %s", len, str);

    return 0;

    strcpy (thread_name, "main");

    // openssl setup
    ERR_load_crypto_strings ();
    if (!p_encrypt_init ()) {
        fprintf (stderr, "FATAL: packet encryption failed to start\n");
        return 1;
    }

    // subthread signal blocking
    sigset_t sig_mask;
    sigemptyset (&sig_mask);
    sigaddset (&sig_mask, SIGINT);

    if (pthread_sigmask (SIG_BLOCK, &sig_mask, NULL)) {
        fprintf (stderr, "FATAL: %s\n", strerror (errno));
        return 1;
    }

    // socket setup
    s_flags |= S_FLAG_NOAUTH;

    s_set_addr4 ("0.0.0.0", 25565);

    if (!s_start ()) {
        fprintf (stderr, "FATAL: socket(s) failed to start\n");
        return 1;
    }

    int sig;
    sigwait (&sig_mask, &sig);

    log ("shutdown signal recv'd (%d)", sig);
    running = false;

    if (s_fd4) close (s_fd4);
    if (s_fd6) close (s_fd6);

    pthread_join (s_thread4, NULL);
    pthread_join (s_thread6, NULL);

    s_cleanup ();

    p_encrypt_free ();
    EVP_cleanup ();
    ERR_free_strings ();

    return 0;
}
