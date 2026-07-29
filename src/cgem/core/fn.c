#include "cgem/core/fn.h"

#include <string.h>

#define CGEM_FN_TYPE "fn"
#define CGEM_FN_PARAM_TYPE "param"

cgem_fn_t *cgem_fn_new(const cgem_char_t *name, cgem_node_t *owner)
{
    return cgem_object_new(name, CGEM_FN_TYPE, owner);
}

size_t cgem_fn_get_param_count(const cgem_fn_t *fn)
{
    size_t count;
    size_t i;
    size_t total = 0;

    if (!fn) {
        return 0;
    }
    count = cgem_node_get_count((const cgem_node_t *) fn);
    for (i = 0; i < count; i++) {
        cgem_node_t *child = cgem_node_get((const cgem_node_t *) fn, i);
        const cgem_char_t *type = cgem_object_get_type(child);

        if (type && strcmp(type, CGEM_FN_PARAM_TYPE) == 0) {
            total++;
        }
    }
    return total;
}

cgem_param_t *cgem_fn_get_param(const cgem_fn_t *fn, size_t index)
{
    size_t count;
    size_t i;
    size_t seen = 0;

    if (!fn) {
        return NULL;
    }
    count = cgem_node_get_count((const cgem_node_t *) fn);
    for (i = 0; i < count; i++) {
        cgem_node_t *child = cgem_node_get((const cgem_node_t *) fn, i);
        const cgem_char_t *type = cgem_object_get_type(child);

        if (type && strcmp(type, CGEM_FN_PARAM_TYPE) == 0) {
            if (seen == index) {
                return child;
            }
            seen++;
        }
    }
    return NULL;
}
