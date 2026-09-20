// lolercraft
// socket and client handling for both ipv4 and ipv6
// includes server handshake status request handling

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <lolercraft/json.h>
#include <lolercraft/logging.h>
#include <lolercraft/protocol.h>

uint8_t s_flags;

// string address
char s_addr4[256], s_addr6[256];

// address struct for ipv4/ipv6
struct sockaddr_in _addr4;
struct sockaddr_in6 _addr6;

// ipv4/ipv6 file descriptors
int s_fd4, s_fd6;

// ipv4/ipv6 ports
uint16_t s_port4, s_port6;

// ipv4/ipv6 server threads
pthread_t s_thread4, s_thread6;

// skip minecraft auth
bool s_offline_mode;

// client backlog
#define BACKLOG 10

// -- client socket handling --

static void _free_client (s_client *client) {
    if (!client) return;

    p_client_free (client);

    if (client->_fd) close (client->_fd);
    if (client->_p_buf) free (client->_p_buf);

    free (client);
}

// callback lists
extern p_sb_cb p_login_sb[5];

static void *_client (void *data) {
    s_client *client = data;
    strcpy (thread_name, client->address);

    // -- client handshake --
    int32_t packet_id = p_receive (client);
    if (packet_id != 0) {
        if (err_state) {
            log_debug ("invalid protocol (0): %s", err_buf);
        } else {
            log_debug ("invalid protocol (wanted 0, got %d)", packet_id);
        }

        _free_client (client);
        return NULL;
    }

    // -- filter invalid connections --
    // (skip addr string, fixed 256)
    if (!(client->_proto = p_read_vint32 (client))
        || !p_read_nbytes (client, NULL, p_read_vint32 (client))
        || p_read_uint16 (client) != (client->_serv6 ? s_port6 : s_port4)
        || !(client->state = p_read_vint32 (client))) {
        if (err_state) {
            log_debug ("client handshake failed (error: %s)", err_buf);
        } else {
            log_debug ("client handshake failed");
        }
        _free_client (client);
        return NULL;
    }

    // -- server status request --
    if (client->state == 1) {
        packet_id = p_receive (client);

        if (client->_encrypted) { log ("enc recv: %d", packet_id); }

        // server status, method can be replaced later but done with raw
        // writes
        if (packet_id == 0) {
            buf_auto *packet_buf = p_buf_init (0);

            pthread_mutex_lock (&_st_mutex);
            char *status_str = status_fetch ();
            p_buf_str (packet_buf, status_str, _st_string_len);
            pthread_mutex_unlock (&_st_mutex);

            if (!p_buf_send (client, packet_buf)) {
                log_debug ("connection error (status:0x0): %s", err_buf);
                _free_client (client);
                return NULL;
            }

            // client will send ping, respond and end
            packet_id = p_receive (client);
            uint8_t ping_resp[8];
            p_read_nbytes (client, ping_resp, 8);

            if (packet_id == 1) {
                packet_buf = p_buf_init (1);
                buf_append (packet_buf, ping_resp, 8);
                if (!p_buf_send (client, packet_buf))
                    log_debug ("connection error (status:0x1): %s", err_buf);
            } else {
                if (err_state) {
                    log_debug ("invalid protocol (1): %s", err_buf);
                } else {
                    log_debug ("invalid protocol (wanted 1, got %d)",
                               packet_id);
                }
            }
        } else {
            if (err_state) {
                log_debug ("invalid protocol (2): %s", err_buf);
            } else {
                log_debug ("invalid protocol (wanted 0, got %d)", packet_id);
            }
        }

        _free_client (client);
        return NULL;
    }

    // -- login handling --
    if (!(client->state == 2 || client->state == 3)) {
        _free_client (client);
        return NULL;
    }

    while (client->state == 2 || client->state == 3) {
        packet_id = p_receive (client);
        if (packet_id == -1
            || packet_id > (sizeof (p_login_sb) / sizeof (p_sb_cb))) {
            // invalid packet
            _free_client (client);
            return NULL;
        }

        if (!p_login_sb[packet_id]) {
            log_err (false, "unhandled packet %d in login state", packet_id);
            // disconnect msg
        } else if (!p_login_sb[packet_id](client)) {
            // if error set send disconnect msg
            if (err_state)
                log_err (false, "login callback 0x%x error: %s", packet_id,
                         err_buf);
            _free_client (client);
            return NULL;
        }
    }

    _free_client (client);
    return NULL;
}

// -- server socket handling --

static void _server (int fd, int proto) {
    int client_fd = 0;
    struct sockaddr client_addr;
    size_t client_addr_len = sizeof (client_addr);

    // 15s client timeout
    struct timeval timeout = (struct timeval) { 15, 0 };

    log ("listening for connections");

    // server client accept loop
    while (true) {
        // accept new client
        if ((client_fd
             = accept (fd, &client_addr, (socklen_t *) &client_addr_len))
            < 0) {
            if (running) log_err (true, "accept failed: %s", strerror (errno));
            break;
        }

        // apply timeout to client
        if (setsockopt (client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                        sizeof timeout)
               < 0
            || setsockopt (client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                           sizeof timeout)
                  < 0) {
            log_err (true, "setsockopt failed: %s", strerror (errno));
            break;
        }

        // set up client struct for new thread
        s_client *client = calloc (1, sizeof (s_client));
        if (!client) { // malloc err
            log_malloc_err (sizeof (s_client));
            raise (SIGINT);
            break;
        }

        client->_serv6 = proto == AF_INET6 ? true : false;
        client->_fd    = client_fd;

        inet_ntop (proto, (const void *) &client_addr, client->address,
                   S_ADDR_LEN_MAX);
        snprintf (client->address + strlen (client->address), 7, ":%d",
                  ntohs (((struct sockaddr_in *) &client_addr)->sin_port));

        // launch thread and keep going
        pthread_create (&client->_thread, NULL, &_client, client);
    }

    log ("socket closed");
}

static void *_thread6 (void *) {
    strcpy (thread_name, "ipv6");
    _server (s_fd6, AF_INET6);

    return NULL;
}

static void *_thread4 (void *) {
    strcpy (thread_name, "ipv4");
    _server (s_fd4, AF_INET);

    return NULL;
}

// launch an instance, non-zero on success
int s_start () {
    // odd variable, only needed for future setsockopt calls
    const int _true = 1;

    // IPv4 and IPv6 are set up similar but require diff. funcs and flags

    // status defaults if not previously set
    status_update_description ("no description set");

    // -- IPv6 setup --
    if (s_flags & S_FLAG_IPV6) {
        log ("initializing IPv6 socket with address %s:%d", s_addr6, s_port6);

        inet_pton (AF_INET6, s_addr6, &(_addr6.sin6_addr));
        _addr6.sin6_port   = htons (s_port6);
        _addr6.sin6_family = AF_INET6;
        s_fd6              = socket (PF_INET6, SOCK_STREAM, IPPROTO_TCP);

        if (s_fd6 < 0) {
            log_err (true, "failed to open IPv6 socket: %s", strerror (errno));
            return 0;
        }

        if (setsockopt (s_fd6, SOL_SOCKET, SO_REUSEADDR, &_true, sizeof (int))
            < 0) {
            log_err (true, "IPv6 setsockopt failed: %s", strerror (errno));
            close (s_fd6);
            return 0;
        }

        if (bind (s_fd6, (const struct sockaddr *) &_addr6,
                  sizeof (struct sockaddr_in6))) {
            log_err (true, "IPv6 bind failed: %s", strerror (errno));
            close (s_fd6);
            return 0;
        }

        if (listen (s_fd6, 10) < 0) {
            log_err (true, "IPv6 listen failed: %s", strerror (errno));
            close (s_fd6);
            return 0;
        }

        pthread_create (&s_thread6, NULL, &_thread6, NULL);
    }

    // -- IPv4 setup --
    if (s_flags & S_FLAG_IPV4) {
        log ("initializing IPv4 socket with address %s:%d", s_addr4, s_port4);

        inet_pton (AF_INET, s_addr4, &(_addr4.sin_addr));
        _addr4.sin_port   = htons (s_port4);
        _addr4.sin_family = AF_INET;
        s_fd4             = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (s_fd4 < 0) {
            log_err (true, "failed to open IPv4 socket: %s", strerror (errno));
            return 0;
        }

        if (setsockopt (s_fd4, SOL_SOCKET, SO_REUSEADDR,
                        ((const void *) &_true), sizeof (int))
            < 0) {
            close (s_fd4);
            log_err (true, "IPv4 setsockopt failed: %s", strerror (errno));
            return 0;
        }

        if (bind (s_fd4, (const struct sockaddr *) &_addr4,
                  sizeof (struct sockaddr_in))) {
            log_err (true, "IPv4 bind failed: %s", strerror (errno));
            close (s_fd4);
            return 0;
        }

        if (listen (s_fd4, 10) < 0) {
            log_err (true, "IPv4 listen failed: %s", strerror (errno));
            close (s_fd4);
            return 0;
        }

        pthread_create (&s_thread4, NULL, &_thread4, NULL);
    }

    return 1;
}

void s_cleanup () { status_cleanup (); }
