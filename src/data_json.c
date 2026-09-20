// lolercraft
// json encoding/decoding

#include <ctype.h>
#include <errno.h>
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
                if (val->data.arr->vals) {
                    for (size_t i = 0; i < val->data.arr->len; i++)
                        _free_vals (val->data.arr->vals[i]);
                    free (val->data.arr->vals);
                }
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

json_array *json_array_init () {
    json_array *arr = calloc (1, sizeof (json_array *));
    if (!arr) { log_malloc_err (sizeof (json_array *)) return NULL; }

    return arr;
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

json_kv *json_array_get (json_array *arr, size_t pos) {
    if (!arr) return NULL;
    if (arr->len <= pos) {
        log_err (false,
                 "json_array_get: index out of bounds (i: %lu, array len: %lu)",
                 pos, arr->len);
        return NULL;
    }
    return arr->vals[pos];
}

json_object *json_get_nested_object (json_object *obj, const char **keys,
                                     size_t len) {
    if (!obj || !keys || !len) return NULL;

    json_kv *kv = json_get (obj, keys[0]);
    if (!kv) return NULL;
    if (kv->type != JSON_OBJECT) {
        log_err (false, "json_get_nested: %s is not a JSON object", keys[0]);
        return NULL;
    }

    if (len == 1) return kv->data.obj;
    return json_get_nested_object (kv->data.obj, &keys[1], len - 1);
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

static bool _array_set (json_array *arr, size_t pos, json_kv *kv) {
    if (!arr || !kv) return false;

    // out of bounds
    if (pos > arr->len) {
        log_err (false,
                 "_array_set: index out of bounds (i: %lu, array len: %lu)",
                 pos, arr->len);
        return false;
    }

    // append to end
    if (pos == arr->len) {
        if (!arr->vals) {
            arr->vals = malloc (sizeof (json_kv **) * 64);
        } else if (((arr->len + 1) & 63) == 0) { // true on multiples of 64
            json_kv **old_ptr = arr->vals;
            size_t new_len
               = sizeof (json_kv **) * 64 * (((arr->len + 1) / 64) + 1);
            arr->vals = realloc (arr->vals, new_len);
            if (!arr->vals) {
                log_malloc_err (new_len);
                free (old_ptr);
                return false;
            }
        }
        arr->vals[pos] = kv;
        arr->len++;
        return true;
    }

    // replace value
    _free_vals (arr->vals[pos]);
    arr->vals[pos] = kv;

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

bool json_array_set_null (json_array *arr, size_t pos) {
    if (!arr) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type = JSON_NULL;

    return _array_set (arr, arr->len, kv);
}

static json_kv *_set_str (const char *str, size_t len) {
    if (!str) return NULL;
    if (!len) len = strlen (str);

    json_kv *kv = _alloc_kv ();
    if (!kv) return NULL;

    kv->type     = JSON_STRING;
    kv->data.str = strndup (str, len);
    if (!kv->data.str) {
        log_malloc_err (len + 1);
        free (kv);
        return NULL;
    }

    // string escapes
    for (size_t i = 0; i < len; i++)
        if (kv->data.str[i] == '"' && kv->data.str[i - 1] == '\\')
            memcpy (kv->data.str + i - 1, kv->data.str + i, len - i + 1);

    return kv;
}

bool json_set_str (json_object *obj, const char *key, const char *str,
                   size_t len) {
    if (!obj) return false;

    json_kv *kv = _set_str (str, len);
    if (!kv) return false;
    return _set (obj, key, kv);
}

bool json_array_set_str (json_array *arr, size_t pos, const char *str,
                         size_t len) {
    if (!arr) return false;

    json_kv *kv = _set_str (str, len);
    if (!kv) return false;
    return _array_set (arr, arr->len, kv);
}

bool json_set_bool (json_object *obj, const char *key, bool b) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_BOOL;
    kv->data.b = b;

    return _set (obj, key, kv);
}

bool json_array_set_bool (json_array *arr, size_t pos, bool b) {
    if (!arr) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_BOOL;
    kv->data.b = b;

    return _array_set (arr, arr->len, kv);
}

bool json_set_num (json_object *obj, const char *key, double num) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_NUM;
    kv->data.n = num;

    return _set (obj, key, kv);
}

bool json_array_set_num (json_array *arr, size_t pos, double num) {
    if (!arr) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type   = JSON_NUM;
    kv->data.n = num;

    return _array_set (arr, arr->len, kv);
}

bool json_set_object (json_object *obj, const char *key, json_object *obj_in) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_OBJECT;
    kv->data.obj = obj_in;

    return _set (obj, key, kv);
}

bool json_array_set_object (json_array *arr, size_t pos, json_object *obj) {
    if (!arr) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_OBJECT;
    kv->data.obj = obj;

    return _array_set (arr, arr->len, kv);
}

bool json_set_array (json_object *obj, const char *key, json_array *arr) {
    if (!obj || !key) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_ARRAY;
    kv->data.arr = arr;

    return _set (obj, key, kv);
}

bool json_array_set_array (json_array *arr, size_t pos, json_array *arr_in) {
    if (!arr) return false;

    json_kv *kv = _alloc_kv ();
    if (!kv) return false;

    kv->type     = JSON_ARRAY;
    kv->data.arr = arr_in;

    return _array_set (arr, arr->len, kv);
}

bool json_remove (json_object *obj, const char *key) { return false; }

// -- parsing --

// false if end of buf
static inline bool _filter_whitespace (const char *buf, size_t *i,
                                       size_t *len) {
    while (true)
        if (*i < *len && isspace (buf[*i])) (*i)++;
        else if (*i == *len || buf[*i] == '\0')
            return false;
        else
            break;

    return true;
}

extern json_object *__decode_obj (const char *buf, size_t *cur, size_t *len);
#define VAL_SET(type, ...)                           \
    (obj ? json_set_##type (obj, key, ##__VA_ARGS__) \
         : json_array_set_##type (arr, arr->len, ##__VA_ARGS__))

static bool _decode_val (const char *buf, size_t *cur, size_t *len,
                         json_object *obj, json_array *arr, const char *key) {
    if (!(arr || (obj && key))) return false;

    size_t i      = *cur;
    int val_start = *cur, val_len = 0;

    // -- parse value by type --

    // alnum values: bools, null, numbers
    if (isalnum (buf[i]) || buf[i] == '-') {
        for (; i < *len; i++) {
            val_len = i - val_start;

            if (buf[i] == ',' || buf[i] == '}' || buf[i] == ']'
                || isspace (buf[i])) {
                i--;
                break;
            }
        }

        if (val_len) {
            // bool (true)
            if (!strncmp ("true", &buf[val_start], val_len)) {
                if (!VAL_SET (bool, true)) { return false; }
            }
            // bool (false)
            else if (!strncmp ("false", &buf[val_start], val_len)) {
                if (!VAL_SET (bool, false)) { return false; }
            }
            // null
            else if (!strncmp ("null", &buf[val_start], val_len)) {
                if (!VAL_SET (null)) { return false; }
            }
            // numerical
            else if (isnumber (buf[val_start]) || buf[val_start] == '-') {
                for (size_t j = 0; j < val_len; j++)
                    if (!(isnumber (buf[j + val_start])
                          || buf[j + val_start] == '.'
                          || (j == 0 && buf[j + val_start] == '-'))) {
                        log_err (
                           false,
                           "json_decode_obj: invalid numerical token '%*.s'",
                           (int) val_len, &buf[val_start])
                    }

                double d;
                if (sscanf (&buf[val_start], "%lf", &d) != 1) {
                    log_err (
                       false,
                       "json_decode_obj: sscanf could not parse double '%*.s'",
                       (int) val_len, &buf[val_start]);
                    return false;
                }

                if (!VAL_SET (num, d)) return false;
            } else {
                log_err (false,
                         "json_decode_obj: incomplete value token '%.*s'",
                         (int) val_len, &buf[val_start]);
                return false;
            }

            *cur = i;
            return true;
        }
    }
    // string
    else if (buf[val_start] == '"') {
        for (; i < *len - 1; i++) {
            if (buf[i] != '\\' && buf[i + 1] == '"') {
                i++;
                break;
            }
        }

        if (!VAL_SET (str, &buf[val_start + 1], i - val_start - 1))
            return false;
    }
    // object
    else if (buf[val_start] == '{') {
        json_object *new_obj = __decode_obj (buf, &i, len);
        *cur                 = i;

        if (!new_obj || !VAL_SET (object, new_obj)) {
            if (!new_obj) json_free (new_obj);
            free (obj);
            return false;
        }
    }
    // array
    else if (buf[i] == '[') {
        i++;

        // filter whitespace
        if (!_filter_whitespace (buf, &i, len)) {
            log_err (false, "json_decode_obj: incomplete array");
            return false;
        }

        // handle empty array cond before loop
        if (buf[i] == ']') {
            i++;
            if (!VAL_SET (array, NULL)) return false;
        } else {
            json_array *new_arr = json_array_init ();
            if (!new_arr) return false;

            while (true) {
                // filter leading whitespace
                if (!_filter_whitespace (buf, &i, len)) {
                    log_err (false, "json_decode_obj: incomplete array");
                    if (new_arr->vals) free (new_arr->vals);
                    free (new_arr);
                    return false;
                }

                if (!_decode_val (buf, &i, len, NULL, new_arr, NULL)) {
                    if (new_arr->vals) free (new_arr->vals);
                    free (new_arr);
                    return false;
                }

                *cur = (++i);

                // filter trailing whitespace
                if (!_filter_whitespace (buf, &i, len)) {
                    log_err (false, "json_decode_obj: incomplete array");
                    if (new_arr->vals) free (new_arr->vals);
                    free (new_arr);
                    return false;
                }

                if (buf[i] == ',') {
                    i++;
                    continue;
                } else if (buf[i] == ']') {
                    i++;
                    break;
                } else {
                    // invalid character
                    log_err (false,
                             "json_decode_obj: expected ',' or ']' char at "
                             "byte %ld, got '%c'",
                             i, buf[i]);
                    if (new_arr->vals) free (new_arr->vals);
                    free (new_arr);
                    return false;
                }
            }

            if (!VAL_SET (array, new_arr)) {
                if (new_arr->vals) free (new_arr->vals);
                free (new_arr);
                return false;
            }
        }
    }
    // unrecognized
    else {
        log_err (false, "json_decode_obj: incomplete value token");
        return false;
    }

    *cur = i;
    return true;
}

#undef VAL_SET

json_object *__decode_obj (const char *buf, size_t *cur, size_t *len) {
    json_object *obj = json_init ();

    uint8_t state  = 0;
    char last_char = 0, key_buf[256] = { 0 };
    size_t key_start = 0;

    for (size_t i = *cur; i < *len; i++) {
        if (!_filter_whitespace (buf, &i, len)) break;

        // OBJECT START {
        if (state == 0) {
            if (buf[i] == '{') {
                state++;
                continue;
            } else {
                log_err (
                   false,
                   "json_decode_obj: expected '{' char at byte %ld, got '%c'",
                   i, buf[i]);
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
                log_err (
                   false,
                   "json_decode_obj: expected '}' char at byte %ld, got '%c'",
                   i, buf[i]);
                json_free (obj);
                return NULL;
            }
        }

        // PULL KEY STR WITH TRAILING "
        if (state == 2) {
            // end key str
            if (last_char != '\\' && buf[i] == '"') {
                if (i - key_start > 255) {
                    log_err (false,
                             "json_decode_obj: key exceeds string limit");
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
                log_err (
                   false,
                   "json_decode_obj: expected ':' char at byte %ld, got '%c'",
                   i, buf[i]);
                json_free (obj);
                return NULL;
            }
        }

        // VALUE
        if (state == 4) {
            if (!_decode_val (buf, &i, len, obj, NULL, key_buf)) {
                json_free (obj);
                return NULL;
            }

            state++;
            *cur = i;
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
                         "json_decode_obj: expected ',' or '}' char at byte "
                         "%ld, got '%c'",
                         i, buf[i]);
                break;
            }
        }
    }

    // ended early
    if (state != 6) {
        if (!err_state) {
            log_err (
               false,
               "json_decode_obj: incomplete object (state: %d, cursor: %lu)",
               state, *cur);
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

    return __decode_obj (buf, &cur, &len);
}

extern bool __append_obj_v (json_kv *v, str_auto *str, bool compact,
                            int indents);

static bool _append_whitespace (str_auto *str, int indents) {
    str_add_char (str, '\n');
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
            str_printf (str, "%lg", v->data.n);
            break;
        }
        case JSON_ARRAY: {
            str_add_char (str, '[');

            if (!v->data.arr || !v->data.arr->vals) {
                str_add_char (str, ']');
                break;
            }

            for (size_t i = 0; i < v->data.arr->len; i++) {
                if (!_append_val_data (v->data.arr->vals[i], str, compact,
                                       indents)) {
                    return false;
                }

                if (i != v->data.arr->len - 1) {
                    str_add_char (str, ',');
                    if (!compact) str_add_char (str, ' ');
                }
            }

            str_add_char (str, ']');
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

    str_add_char (str, '{');

    for (json_kv *val = v; val;) {
        if (!compact) _append_whitespace (str, indents);
        str_printf (str, compact ? "\"%s\":" : "\"%s\": ", val->key);
        _append_val_data (val, str, compact, indents);

        val = val->_next;
        if (val) str_add_char (str, ',');
    }

    if (!compact) _append_whitespace (str, indents - 1);
    str_add_char (str, '}');

    return true;
}

char *json_encode (json_object *obj, size_t *len, bool compact) {
    if (!obj) return NULL;

    str_auto *str = str_init (NULL);
    if (!str) return NULL; // malloc error will print within str_init

    if (!__append_obj_v (obj->vals, str, compact, 0)) {
        free (str);
        return NULL;
    }

    // ditch the auto string and return
    return str_detach (str, len);
}

// -- file i/o --
bool json_write (json_object *obj, const char *path) {
    FILE *file = fopen (path, "w");
    if (!file) {
        log_err (false, "json_write: fopen failed: %s", strerror (errno));
        return false;
    }

    size_t file_len;
    char *buf = json_encode (obj, &file_len, false);
    if (!buf) {
        fclose (file);
        return false;
    }

    if (!fwrite (buf, file_len, 1, file)) {
        log_err (false, "json_write: fwrite failed: %s", strerror (errno));
        fclose (file);
        free (buf);
        return false;
    }

    fclose (file);
    free (buf);
    return true;
}

json_object *json_read (const char *path) {
    FILE *file = fopen (path, "r");
    if (!file) {
        log_err (false, "json_read: fopen failed: %s", strerror (errno));
        return NULL;
    }

    fseek (file, 0, SEEK_END);
    size_t file_len = ftell (file);
    fseek (file, 0, SEEK_SET);

    char *buf = malloc (file_len + 1);
    if (!buf) {
        log_malloc_err (file_len + 1);
        fclose (file);
        return NULL;
    }

    buf[file_len - 1] = '\0';

    if (fread ((void *) buf, 1, file_len, file) != file_len) {
        log_err (false, "json_read: fread failed: %s", strerror (errno));
        free (buf);
        fclose (file);
        return NULL;
    }

    fclose (file);

    json_object *obj = json_decode (buf, file_len);

    free (buf);
    return obj;
}
