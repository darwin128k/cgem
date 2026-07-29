#ifndef CGEM_ATTRIBUTE_H
#define CGEM_ATTRIBUTE_H

#include "cgem/core/primitive.h"

#include <stddef.h>

typedef enum {
    ATTR_VALUE_NULL,
    ATTR_VALUE_BOOL,
    ATTR_VALUE_INT,
    ATTR_VALUE_FLOAT,
    ATTR_VALUE_STRING,
    ATTR_VALUE_SYMBOL,
    ATTR_VALUE_LIST
} cgem_attribute_value_kind_t;

typedef struct cgem_attribute_value cgem_attribute_value_t;
typedef struct cgem_attribute cgem_attribute_t;

cgem_attribute_value_t *cgem_attribute_value_new_null(void);
cgem_attribute_value_t *cgem_attribute_value_new_bool(cgem_bool_t value);
cgem_attribute_value_t *cgem_attribute_value_new_int(cgem_llong_t value);
cgem_attribute_value_t *cgem_attribute_value_new_float(cgem_double_t value);
cgem_attribute_value_t *cgem_attribute_value_new_string(
    const cgem_char_t *value);
cgem_attribute_value_t *cgem_attribute_value_new_symbol(
    const cgem_char_t *value);
cgem_attribute_value_t *cgem_attribute_value_new_list(void);
void cgem_attribute_value_free(cgem_attribute_value_t *value);

cgem_bool_t cgem_attribute_value_list_append(cgem_attribute_value_t *list,
                                             cgem_attribute_value_t *item);

cgem_attribute_value_kind_t cgem_attribute_value_get_kind(
    const cgem_attribute_value_t *value);
cgem_bool_t cgem_attribute_value_get_bool(const cgem_attribute_value_t *value);
cgem_llong_t cgem_attribute_value_get_int(const cgem_attribute_value_t *value);
cgem_double_t cgem_attribute_value_get_float(
    const cgem_attribute_value_t *value);
const cgem_char_t *cgem_attribute_value_get_string(
    const cgem_attribute_value_t *value);
size_t cgem_attribute_value_list_get_count(const cgem_attribute_value_t *value);
const cgem_attribute_value_t *cgem_attribute_value_list_get(
    const cgem_attribute_value_t *value, size_t index);

/* Size of the value's own content in bytes: 0 for NULL, sizeof() of the
 * underlying scalar for BOOL/INT/FLOAT, string length + 1 for STRING/SYMBOL,
 * sum of element sizes (recursively) for LIST. */
size_t cgem_attribute_value_get_size(const cgem_attribute_value_t *value);

cgem_attribute_t *cgem_attribute_new(const cgem_char_t *key,
                                     cgem_attribute_value_t *value);
void cgem_attribute_free(cgem_attribute_t *attribute);
const cgem_char_t *cgem_attribute_get_key(const cgem_attribute_t *attribute);
const cgem_attribute_value_t *cgem_attribute_get_value(
    const cgem_attribute_t *attribute);

#endif
