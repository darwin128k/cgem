#ifndef CGEM_ATTRIBUTE_H
#define CGEM_ATTRIBUTE_H

#include <stdbool.h>
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
cgem_attribute_value_t *cgem_attribute_value_new_bool(bool value);
cgem_attribute_value_t *cgem_attribute_value_new_int(long long value);
cgem_attribute_value_t *cgem_attribute_value_new_float(double value);
cgem_attribute_value_t *cgem_attribute_value_new_string(const char *value);
cgem_attribute_value_t *cgem_attribute_value_new_symbol(const char *value);
cgem_attribute_value_t *cgem_attribute_value_new_list(void);
void cgem_attribute_value_free(cgem_attribute_value_t *value);

bool cgem_attribute_value_list_append(cgem_attribute_value_t *list,
                                      cgem_attribute_value_t *item);

cgem_attribute_value_kind_t cgem_attribute_value_get_kind(
    const cgem_attribute_value_t *value);
bool cgem_attribute_value_get_bool(const cgem_attribute_value_t *value);
long long cgem_attribute_value_get_int(const cgem_attribute_value_t *value);
double cgem_attribute_value_get_float(const cgem_attribute_value_t *value);
const char *cgem_attribute_value_get_string(const cgem_attribute_value_t *value);
size_t cgem_attribute_value_list_get_count(const cgem_attribute_value_t *value);
const cgem_attribute_value_t *cgem_attribute_value_list_get(
    const cgem_attribute_value_t *value, size_t index);

cgem_attribute_t *cgem_attribute_new(const char *key,
                                     cgem_attribute_value_t *value);
void cgem_attribute_free(cgem_attribute_t *attribute);
const char *cgem_attribute_get_key(const cgem_attribute_t *attribute);
const cgem_attribute_value_t *cgem_attribute_get_value(
    const cgem_attribute_t *attribute);

#endif
