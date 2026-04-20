//
// Created by andrew on 4/10/26.
//

#include <asm-generic/types.h>
#include <stdio.h>
#include "json-c/json.h"
#include <string.h>
#include <stdbool.h>
#include "json_helper.h"
#include "generic.h"

json_object *json_get_from_file(const char *fname, char **buffer) {
    FILE *f = fopen(fname, "r");
    if (!f) {
        printf("Failed reading from configuration file %s\n", fname);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buffer = calloc(1, size);
    if (!*buffer) {
        printf("Failed allocating for configuration buffer\n");
        fclose(f);
        return NULL;
    }
    if (fread(*buffer, 1, size, f) <= 0) {
        printf("Failed reading configuration file into buffer\n");
        FREE_NULL(*buffer);
        return NULL;
    }
    fclose(f);
    json_object *obj = json_tokener_parse(*buffer);
    if (!obj) {
        printf("Failed json parsing of buffer\n");
    }
    FREE_NULL(*buffer);
    return obj;
}

int json_get_int64(json_object *obj, const char *path, __u64 *target) {
    json_object *ptr;
    if (json_pointer_get(obj, path, &ptr) < 0)
        return -1;
    *target = (__u64)json_object_get_int64(ptr);
    return 0;
}

int json_get_int(json_object *obj, const char *path, __u32 *target) {
    json_object *ptr;
    if (json_pointer_get(obj, path, &ptr) < 0)
        return -1;
    *target = (__u32)json_object_get_int(ptr);
    return 0;
}

int json_get_u16(json_object *obj, const char *path, __u16 *target) {
    json_object *ptr;
    if (json_pointer_get(obj, path, &ptr) < 0)
        return -1;
    *target = (__u16)json_object_get_int(ptr);
    return 0;
}

int json_get_str(json_object *obj, const char *path, char **str) {
    json_object *ptr = NULL;
    const char *tmpstr = NULL;
    if (json_pointer_get(obj, path, &ptr) < 0) {
        return -1;
    }
    tmpstr = json_object_get_string(ptr);
    if (tmpstr) {
        *str = calloc(1, strlen(tmpstr)+1);
        if (!*str)
            return -1;
        memcpy(*str, tmpstr, strlen(tmpstr)+1);
        return 0;
    }
    return -1;
}

int json_get_bool(json_object *obj, const char *path, bool *value) {
    json_object *ptr = NULL;
    if (json_pointer_get(obj, path, &ptr) < 0)
        return -1;
    *value = json_object_get_boolean(ptr);
    return 0;
}

void json_free(json_object *obj) {
    if (!obj)
        return;
    json_object_put(obj);
}