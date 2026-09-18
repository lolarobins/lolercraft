// lolercraft
// json encoding/decoding

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lolercraft/json.h>
#include <lolercraft/logging.h>
#include <lolercraft/misctypes.h>

// -- free & init --

static void _free_vals (json_kv *v) {
    for (json_kv *val = v; val != NULL;) {
        switch (val->type) {

            case JSON_STRING: {
                free (val->data.str);
                break;
            }
            case JSON_ARRAY: {
                for (size_t i = 0; i < val->data.arr->len; i++)
                    _free_vals (val->data.arr->vals[i]);
                free (val->data.arr);
                break;
            }
            case JSON_OBJECT: {
                json_free (val->data.obj);
                break;
            }
            default: break;
        }

        json_kv *old_ptr = val;
        val              = val->_next;
        free (old_ptr);
    }
}

void json_free (json_object *obj) {
    if (!obj || obj->static_alloc) return;

    _free_vals (obj->vals);

    free (obj);
}

json_object *json_init () {
    json_object *obj = calloc (1, sizeof (json_object *));
    if (!obj) { log_malloc_err (sizeof (json_object *)) return NULL; }

    return obj;
}

// -- json modification functions --

json_kv *json_get (json_object *obj, const char *key) {
    if (!obj || !key) return NULL;

    for (json_kv *val = obj->vals; val; val = val->_next) {
        if (!strcmp (val->key, key)) return val;
    }

    log_err (false, "json_get: %s is not defined", key);
    return NULL;
}

json_object *json_nested_object (json_object *obj, const char **keys,
                                 size_t len) {
    if (!obj || !keys || !len) return NULL;

    json_kv *kv = json_get (obj, keys[0]);
    if (!kv) return NULL;
    if (kv->type != JSON_OBJECT) {
        log_err (false, "json_get_nested: %s is not a JSON object", keys[0]);
        return NULL;
    }

    if (len == 1) return kv->data.obj;
    return json_nested_object (kv->data.obj, &keys[1], len - 1);
}

static bool _set (json_object *obj, const char *key, json_kv *data) {
    // existing key, and key previous to it for ll stuff
    json_kv *existing_key = NULL;

    if (obj->vals)
        for (json_kv *kv = obj->vals; kv; kv = kv->_next)
            if (!strcmp (kv->key, key)) {
                existing_key = kv;
                break;
            }

    if (!existing_key) {
        strncpy (data->key, key, 256);

        if (!obj->vals) obj->vals = data;
        else
            for (json_kv *kv = obj->vals;; kv = kv->_next) {
                if (kv->_next == NULL) {
                    kv->_next = data;
                    break;
                }
            }
    } else {
        switch (existing_key->type) {
            case JSON_OBJECT: {
                free (existing_key->data.obj);
            }
            case JSON_STRING: {
                free (existing_key->data.obj);
            }
            // !! ADD ARRAY CASE
            default: {
                break;
            }
        }

        memcpy (&existing_key->data, &data->data, sizeof (data->data));
        free (data);
    }

    return true;
}

static json_kv *_alloc_kv () {
    json_kv *kv = calloc (1, sizeof (json_kv));
    if (!kv) {
        log_malloc_err (sizeof (json_kv));
        return NULL;
    }

    return kv;
}

bool json_set_null (json_object *obj, const char *key) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type = JSON_NULL;

    return _set (obj, key, kv);
}

bool json_set_str (json_object *obj, const char *key, const char *str,
                   size_t len) {
    if (!obj || !key || !str) return false;
    if (!len) len = strlen (str);

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_STRING;
    kv->data.str = strndup (str, len);
    if (!kv->data.str) {
        log_malloc_err (len + 1);
        free (kv);
        return false;
    }

    // string escapes
    for (size_t i = 0; i < len; i++)
        if (kv->data.str[i] == '"' && kv->data.str[i - 1] == '\\')
            memcpy (kv->data.str + i - 1, kv->data.str + i, len - i + 1);

    return _set (obj, key, kv);
}

bool json_set_bool (json_object *obj, const char *key, bool b) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_BOOL;
    kv->data.b = b;

    return _set (obj, key, kv);
}

bool json_set_num (json_object *obj, const char *key, double num) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_NUM;
    kv->data.n = num;

    return _set (obj, key, kv);
}

bool json_set_object (json_object *obj, const char *key, json_object *obj_in) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_OBJECT;
    kv->data.obj = obj_in;

    return _set (obj, key, kv);
    return true;
}

// !! this will be a bit more complicated, do later
bool json_set_array (json_object *obj, const char *key, json_kv *data,
                     size_t len) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type = JSON_ARRAY;

    return false;
}

bool json_remove (json_object *obj, const char *key) { return false; }

static json_object *_decode (const char *buf, size_t *cur, size_t *len) {
    json_object *obj = json_init ();

    uint8_t state  = 0;
    char last_char = 0, key_buf[256] = { 0 };
    size_t key_start = 0, val_start = 0, val_len = 0;

    for (size_t i = *cur; i < *len; i++) {
        if (buf[i] == '\0') break;
        while (i < *len && isspace (buf[i])) i++;

        // OBJECT START {
        if (state == 0) {
            if (buf[i] == '{') {
                state++;
                continue;
            } else {
                log_err (false, "json_decode: expected '{' char at byte %ld",
                         i);
                json_free (obj);
                return NULL;
            }
        }

        // KEY START " (REPEATS)
        if (state == 1) {
            if (buf[i] == '"') {
                state++;
                key_start = i + 1;
                continue;
            } else if (buf[i] == '}') {
                state = 6;
                break;
            } else {
                log_err (false, "json_decode: expected '}' char at byte %ld",
                         i);
                json_free (obj);
                return NULL;
            }
        }

        // PULL KEY STR WITH TRAILING "
        if (state == 2) {
            // end key str
            if (last_char != '\\' && buf[i] == '"') {
                if (i - key_start > 255) {
                    log_err (false, "json_decode: key exceeds string limit");
                    json_free (obj);
                    return NULL;
                }

                memcpy (key_buf, &buf[key_start], i - key_start);
                key_buf[i - key_start] = '\0';
                state++;

                continue;
            }

            last_char = buf[i - 1];
            continue;
        }

        // COLON AFTER KEY
        if (state == 3) {
            if (buf[i] == ':') {
                state++;
                continue;
            } else {
                log_err (false, "json_decode: expected ':' char at byte %ld",
                         i);
                json_free (obj);
                return NULL;
            }
        }

        // VALUE
        if (state == 4) {
            val_start = i;
            val_len   = 0;

            // -- get value--

            // alnum values: bools, null, numbers
            if (isalnum (buf[i]) || buf[i] == '-') {
                for (; i < *len; i++) {
                    val_len = i - val_start;

                    if (buf[i] == ',' || buf[i] == '}' || isspace (buf[i])) {
                        i--;
                        break;
                    }
                }

                // -- parse values --

                if (val_len) {
                    // bool (true)
                    if (!strncmp ("true", &buf[val_start], val_len)) {
                        if (!json_set_bool (obj, key_buf, true)) { break; }
                    }
                    // bool (false)
                    else if (!strncmp ("false", &buf[val_start], val_len)) {
                        if (!json_set_bool (obj, key_buf, false)) { break; }
                    }
                    // null
                    else if (!strncmp ("null", &buf[val_start], val_len)) {
                        if (!json_set_null (obj, key_buf)) { break; }
                    }
                    // numerical
                    else if (isnumber (buf[val_start]
                                       || buf[val_start] == '-')) {
                    } else {
                        log_err (false,
                                 "json_decode: incomplete value token (key: "
                                 "%s, token: %.*s)",
                                 key_buf, (int) val_len, &buf[val_start]);
                    }
                }

                state++;
                continue;
            }

            // string
            if (buf[val_start] == '"') {
                for (; i < *len - 1; i++) {
                    if (buf[i] != '\\' && buf[i + 1] == '"') {
                        i++;
                        break;
                    }
                }

                json_set_str (obj, key_buf, &buf[val_start + 1],
                              i - val_start - 1);
            }
            // object
            else if (buf[val_start] == '{') {
                *cur = i;

                json_object *new_obj = _decode (buf, cur, len);

                if (!new_obj || !json_set_object (obj, key_buf, new_obj)) {
                    if (!new_obj) json_free (new_obj);
                    free (obj);
                    return NULL;
                }

                i = *cur - 1;
            }
            // array
            else if (buf[i] == '[') {
            }
            // unrecognized
            else {
                log_err (false,
                         "json_decode: incomplete value token (key: "
                         "%s)",
                         key_buf);
            }

            state++;
            continue;
        }

        // COMMA = STATE 1, CURLY BRACKET CLOSE = FINISH
        if (state == 5) {
            if (buf[i] == ',') {
                // onto next kv pair
                *cur  = i + 1;
                state = 1;
                continue;
            } else if (buf[i] == '}') {
                // finished :3
                *cur = i + 1;
                state++; // 6 state signifies success on loop end
                break;
            } else if (err_state) {
                log_err (false,
                         "json_decode: expected ',' or '}' char at byte %ld",
                         i);
                break;
            }
        }
    }

    // ended early
    if (state != 6) {
        if (err_state) {
            log_err (false, "%s, json_decode: incomplete object (state: %d)",
                     err_buf, state)
        } else {
            log_err (false, "json_decode: incomplete object (state: %d)",
                     state);
        }
        json_free (obj);
        return NULL;
    }

    return obj;
}

json_object *json_decode (const char *buf, size_t len) {
    if (!buf) return NULL;

    if (!len) len = strlen (buf);
    size_t cur = 0;

    return _decode (buf, &cur, &len);
}

extern bool __append_obj_v (json_kv *v, str_auto *str, bool compact,
                            int indents);

static bool _append_whitespace (str_auto *str, int indents) {
    str_putchar (str, '\n');
    for (int i = 0; i < indents + 1; i++) { str_append (str, "    "); }

    return true;
}

static bool _append_val_data (json_kv *v, str_auto *str, bool compact,
                              int indents) {
    switch (v->type) {
        case JSON_NULL: {
            str_append (str, "null");
            break;
        }
        case JSON_STRING: {
            if (v->data.str) str_printf (str, "\"%s\"", v->data.str);
            else
                str_append (str, "\"\"");

            break;
        }
        case JSON_BOOL: {
            str_append (str, v->data.b ? "true" : "false");
            break;
        }
        case JSON_NUM: {
            if (fmod (v->data.n, 1.0)) {
                str_printf (str, "%lf", v->data.n);
            } else
                str_printf (str, "%ld", (long) v->data.n);
            break;
        }
        case JSON_ARRAY: {
            str_putchar (str, '[');

            if (!v->data.arr || !v->data.arr->vals) {
                str_putchar (str, ']');
                break;
            }

            for (size_t i = 0; i < v->data.arr->len; i++) {
                str += _append_val_data (v->data.arr->vals[i], str, compact,
                                         indents);
                if (i != v->data.arr->len + 1) {
                    str_putchar (str, ',');
                    if (!compact) str_putchar (str, ' ');
                }
            }

            str_putchar (str, ']');
            break;
        }
        case JSON_OBJECT: {
            if (!v->data.obj || !v->data.obj->vals) {
                str_append (str, "{}");
            } else {
                str += __append_obj_v (v->data.obj->vals, str, compact,
                                       indents + 1);
            }
            break;
        }
    }

    return true;
}

bool __append_obj_v (json_kv *v, str_auto *str, bool compact, int indents) {
    if (!v || !str) return 0;

    str_putchar (str, '{');

    for (json_kv *val = v; val;) {
        if (!compact) _append_whitespace (str, indents);
        str_printf (str, compact ? "\"%s\":" : "\"%s\": ", val->key);
        _append_val_data (val, str, compact, indents);

        val = val->_next;
        if (val) str_putchar (str, ',');
    }

    if (!compact) _append_whitespace (str, indents - 1);
    str_putchar (str, '}');

    return true;
}

char *json_encode (json_object *obj, size_t *len, bool compact) {
    if (!obj) return NULL;

    str_auto *str = str_init (NULL);
    if (!str) return NULL; // malloc error will print within str_init

    __append_obj_v (obj->vals, str, compact, 0);

    // ditch the auto string and return
    return str_detach (str, len);
}
