// lolercraft
// json encoding/decoding

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

// NOTES:
// - in json_kv struct, key len is hardcoded to 256B

typedef struct json_kv json_kv;
typedef struct json_object json_object;
typedef struct json_array json_array;

typedef union json_data_union {
    char *str;
    json_object *obj;
    json_array *arr;
    double n;
    bool b;
} json_data_union;

typedef enum json_type {
    JSON_NULL,
    JSON_STRING,
    JSON_BOOL,
    JSON_NUM,
    JSON_ARRAY,
    JSON_OBJECT
} json_type;

typedef struct json_kv {
    // linked list
    json_kv *_next;

    char key[256];
    json_type type;
    json_data_union data;
} json_kv;

typedef struct json_object {
    json_kv *vals;
    // do not free if statically allocated
    bool static_alloc;
} json_object;

typedef struct json_array {
    json_kv **vals;
    size_t len;
} json_array;

// free a json object
extern void json_free (json_object *obj);
// init a new json object
extern json_object *json_init ();
// get a kv pair by path/key
extern json_kv *json_get (json_object *obj, const char *key);
// get a kv pair nested in object or array values named by key
extern json_object *json_nested_object (json_object *obj, const char **keys,
                                        size_t len);
// set key to null
extern bool json_set_null (json_object *obj, const char *key);
// set key to string val
extern bool json_set_str (json_object *obj, const char *key, const char *str, size_t len);
// set key to bool val
extern bool json_set_bool (json_object *obj, const char *key, bool b);
// set key to numerical val
extern bool json_set_num (json_object *obj, const char *key, double num);
// set key to object
extern bool json_set_object (json_object *obj, const char *key,
                             json_object *obj_in);
// set key to an array
extern bool json_set_array (json_object *obj, const char *key, json_kv *data,
                            size_t len);
// remove a value by path/key
extern bool json_remove (json_object *obj, const char *key);
// decode a string into a json object
json_object *json_decode (const char *buf, size_t len);
// encode a json object to a string
extern char *json_encode (json_object *obj, size_t *len, bool compact);

#ifdef __cplusplus
}
#endif
