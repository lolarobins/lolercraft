// lolercraft
// json encoding/decoding

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

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

// note: key len is hardcoded to 256B (at the moment!)
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
// init a new json array
extern json_array *json_array_init ();

// get a kv pair by path/key
extern json_kv *json_get (json_object *obj, const char *key);
// get a kv pair in an array at a position
extern json_kv *json_array_get (json_array *arr, size_t pos);
// get a nested object
extern json_object *json_get_nested_object (json_object *obj, const char **keys,
                                            size_t len);

// set key to null
extern bool json_set_null (json_object *obj, const char *key);
// set key to string val
extern bool json_set_str (json_object *obj, const char *key, const char *str,
                          size_t len);
// set key to bool val
extern bool json_set_bool (json_object *obj, const char *key, bool b);
// set key to numerical val
extern bool json_set_num (json_object *obj, const char *key, double num);
// set key to object
extern bool json_set_object (json_object *obj, const char *key,
                             json_object *obj_in);
// set key to an array
extern bool json_set_array (json_object *obj, const char *key, json_array *arr);

// set array val to null
extern bool json_array_set_null (json_array *arr, size_t pos);
// set array val to a string val
extern bool json_array_set_str (json_array *arr, size_t pos, const char *str,
                                size_t len);
// set array val to a bool val
extern bool json_array_set_bool (json_array *arr, size_t pos, bool b);
// set array val to a numerical val
extern bool json_array_set_num (json_array *arr, size_t pos, double num);
// set array val to an object
extern bool json_array_set_object (json_array *arr, size_t pos,
                                   json_object *obj);
// set array val to another array
extern bool json_array_set_array (json_array *arr, size_t pos,
                                  json_array *arr_in);

// remove a value by path/key
extern bool json_remove (json_object *obj, const char *key);

// decode a string into a json object
json_object *json_decode (const char *buf, size_t len);
// encode a json object to a string
extern char *json_encode (json_object *obj, size_t *len, bool compact);

// write a json object to a file
extern bool json_write (json_object *obj, const char *path);
// read a json file
extern json_object *json_read (const char *path);

#ifdef __cplusplus
}
#endif
