// lolercraft

#include "lolercraft/json.h"
#include "lolercraft/socket.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include <stdio.h>
#include <string.h>
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

static const char default_config[]
   = "{\n"
     "    \"offline\": false,\n"
     "    \"ipv4\": {\n"
     "        \"address\": \"localhost\",\n"
     "        \"port\": 25565,\n"
     "        \"enabled\": true\n"
     "    },\n"
     "    \"ipv6\": {\n"
     "        \"address\": \"::1\",\n"
     "        \"port\": 25565,\n"
     "        \"enabled\": false\n"
     "    }\n"
     "}\n";

static bool load_config () {
    json_object *obj = json_read ("config.json");
    if (!obj) {
        if (errno == ENOENT) {
            errno = 0;

            // create file
            FILE *file = fopen ("config.json", "w");
            if (!file) {
                fprintf (stderr,
                         "FATAL: could not open config.json for write: %s\n",
                         strerror (errno));
                return false;
            }

            fwrite (default_config, sizeof (default_config) - 1, 1, file);
            if (errno) {
                fprintf (stderr, "FATAL: could not write to config.json: %s\n",
                         strerror (errno));
                fclose (file);
                return false;
            }

            fclose (file);

            char full_path[PATH_MAX];
            if (!realpath ("config.json", full_path)) {
                fprintf (
                   stderr,
                   "FATAL: could not determine config.json file path: %s\n",
                   strerror (errno));
                return -1;
            }

            printf (
               "default configuration generated at %s, please make any "
               "necessary changes and restart the program\n",
               full_path);
            return false;
        } else {
            fprintf (stderr, "FATAL: failed to read config.json: %s\n",
                     err_buf);
            return false;
        }
    }

    // TODO: implement json auto loading values to avoid this repetitive thing

    // load values
    json_kv *offline_mode = json_get (obj, "offline");
    s_offline_mode        = offline_mode && offline_mode->type == JSON_BOOL
                            && offline_mode->data.b == true;

    json_kv *ipv4 = json_get (obj, "ipv4");
    if (ipv4 && ipv4->type == JSON_OBJECT) {
        json_kv *enabled = json_get (ipv4->data.obj, "enabled");
        if (enabled && enabled->type == JSON_BOOL && enabled->data.b == true) {
            s_flags |= S_FLAG_IPV4;

            json_kv *address = json_get (ipv4->data.obj, "address");
            if (!address || address->type != JSON_STRING)
                strcpy (s_addr4, "localhost");
            else
                strncpy (s_addr4, address->data.str, sizeof (s_addr4));

            json_kv *port = json_get (ipv4->data.obj, "port");
            if (!port || port->type != JSON_NUM) s_port4 = 25565;
            else
                s_port4 = (uint16_t) port->data.n;
        }
    }

    json_kv *ipv6 = json_get (obj, "ipv6");
    if (ipv6 && ipv6->type == JSON_OBJECT) {
        json_kv *enabled = json_get (ipv6->data.obj, "enabled");
        if (enabled && enabled->type == JSON_BOOL && enabled->data.b == true) {
            s_flags |= S_FLAG_IPV6;

            json_kv *address = json_get (ipv6->data.obj, "address");
            if (!address || address->type != JSON_STRING)
                strcpy (s_addr6, "localhost");
            else
                strncpy (s_addr6, address->data.str, sizeof (s_addr6));

            json_kv *port = json_get (ipv6->data.obj, "port");
            if (!port || port->type != JSON_NUM) s_port6 = 25565;
            else
                s_port6 = (uint16_t) port->data.n;
        }
    }

    json_free (obj);

    // debug print
    log_debug ("starting with the following configuration:");
    log_debug (" - mojang authentication: %s",
               s_offline_mode ? "disabled" : "enabled");
    if (s_flags & S_FLAG_IPV4)
        log_debug (" - ipv4 socket: %s:%d", s_addr4, s_port4);
    if (s_flags & S_FLAG_IPV6)
        log_debug (" - ipv6 socket: %s:%d", s_addr6, s_port6);

    return true;
}

int main () {
    strcpy (thread_name, "main");

    // load config
    if (!load_config ()) return -1;

    // verify existence of at least 1 socket
    if ((s_flags & (S_FLAG_IPV4 | S_FLAG_IPV6)) == 0) {
        fprintf (stderr, "FATAL: neither ipv4 nor ipv6 socket enabled\n");
        return -1;
    }

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
