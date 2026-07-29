#include "cgem/core/fn.h"

#include "cgem/core/attributes.h"

#include <string.h>

#define CGEM_FN_TYPE "fn"
#define CGEM_FN_PARAM_TYPE "param"
#define CGEM_FN_RETURN_KEY "return"

cgem_fn_t *cgem_fn_new(const cgem_char_t *name,
                       cgem_attribute_value_t *return_type,
                       cgem_node_t *owner)
{
    cgem_node_t *node = cgem_object_new(name, CGEM_FN_TYPE, owner);
    cgem_attribute_t *attribute;

    if (!node) {
        cgem_attribute_value_free(return_type);
        return NULL;
    }
    attribute = cgem_attribute_new(CGEM_FN_RETURN_KEY, return_type);
    if (!attribute) {
        cgem_attribute_value_free(return_type);
        cgem_node_free(node);
        return NULL;
    }
    if (!cgem_attributes_add(cgem_node_get_attributes(node), attribute)) {
        cgem_attribute_free(attribute);
        cgem_node_free(node);
        return NULL;
    }
    return node;
}

const cgem_attribute_value_t *cgem_fn_get_return_type(const cgem_fn_t *fn)
{
    cgem_attributes_t *attributes;
    const cgem_attribute_t *attribute;

    if (!fn) {
        return NULL;
    }
    attributes = cgem_node_get_attributes((cgem_node_t *) fn);
    attribute = cgem_attributes_find(attributes, CGEM_FN_RETURN_KEY);
    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_get_value(attribute);
}

cgem_bool_t cgem_fn_set_return_type(cgem_fn_t *fn,
                                    cgem_attribute_value_t *return_type)
{
    cgem_attribute_t *attribute;

    if (!fn) {
        cgem_attribute_value_free(return_type);
        return false;
    }
    attribute = cgem_attribute_new(CGEM_FN_RETURN_KEY, return_type);
    if (!attribute) {
        cgem_attribute_value_free(return_type);
        return false;
    }
    if (!cgem_attributes_add(cgem_node_get_attributes((cgem_node_t *) fn),
                             attribute)) {
        cgem_attribute_free(attribute);
        return false;
    }
    return true;
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

size_t cgem_fn_get_statement_count(const cgem_fn_t *fn)
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

        if (!(type && strcmp(type, CGEM_FN_PARAM_TYPE) == 0)) {
            total++;
        }
    }
    return total;
}

cgem_node_t *cgem_fn_get_statement(const cgem_fn_t *fn, size_t index)
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

        if (!(type && strcmp(type, CGEM_FN_PARAM_TYPE) == 0)) {
            if (seen == index) {
                return child;
            }
            seen++;
        }
    }
    return NULL;
}
