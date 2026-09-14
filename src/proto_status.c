// lolercraft
// server status handling

#include <pthread.h>

#include <lolercraft/json.h>
#include <lolercraft/logging.h>

static json_kv st_secure
   = { .key = "enforcesSecureChat", .type = JSON_BOOL, .data.b = false };

static json_kv st_icon
   = { .key = "favicon", .type = JSON_STRING, ._next = &st_secure };

// description, type of text component (fix that later)
static json_kv st_desc_text = { .key = "text", .type = JSON_STRING };

static json_object st_desc_obj
   = { .vals = &st_desc_text, .static_alloc = true };
static json_kv st_desc = { .key      = "description",
                           .type     = JSON_OBJECT,
                           .data.obj = &st_desc_obj,
                           ._next    = &st_icon };

static json_kv st_players_max
   = { .key = "max", .type = JSON_NUM, .data.n = 64 };
static json_kv st_players_online = { .key    = "online",
                                     .type   = JSON_NUM,
                                     .data.n = 1,
                                     ._next  = &st_players_max };
static json_kv st_players_sample
   = { .key = "sample", .type = JSON_ARRAY, ._next = &st_players_online };

static json_object st_players_obj
   = { .vals = &st_players_sample, .static_alloc = true };
static json_kv st_players = { .key      = "players",
                              .type     = JSON_OBJECT,
                              .data.obj = &st_players_obj,
                              ._next    = &st_desc };

static json_kv st_vers_name
   = { .key = "name", .type = JSON_STRING, .data.str = "lolercraft" };
static json_kv st_vers_proto = { .key    = "protocol",
                                 .type   = JSON_NUM,
                                 .data.n = 775,
                                 ._next  = &st_vers_name };

static json_object st_vers_obj
   = { .vals = &st_vers_proto, .static_alloc = true };
static json_kv st_vers = { .key      = "version",
                           .type     = JSON_OBJECT,
                           .data.obj = &st_vers_obj,
                           ._next    = &st_players };

json_object st_root = { .vals = &st_vers, .static_alloc = true };

// 3s cache
pthread_mutex_t _st_mutex = PTHREAD_MUTEX_INITIALIZER;
time_t _st_expire;
char *_st_string;
size_t _st_string_len;

// must be used with _st_mutex locked
char *status_fetch () {
    if (!_st_string || time (NULL) > _st_expire) {
        if (_st_string) free (_st_string);

        _st_string = json_encode (&st_root, &_st_string_len, true);
        _st_expire = time (NULL) + 3;

        if (!_st_string) {
            log_err (true, "server status failed to generate: %s", err_buf);
            return NULL;
        }
    }

    return _st_string;
}

bool status_update_description (const char *desc) {
    if (!desc) return false;

    pthread_mutex_lock (&_st_mutex);
    if (st_desc_text.data.str) { free (st_desc_text.data.str); }

    st_desc_text.data.str = strdup (desc);
    if (!st_desc_text.data.str) {
        log_malloc_err (strlen (desc) + 1);
        return false;
    }
    pthread_mutex_unlock (&_st_mutex);

    return true;
}

void status_cleanup () {
    if (_st_string) free (_st_string);
    if (st_desc_text.data.str) free (st_desc_text.data.str);
}
