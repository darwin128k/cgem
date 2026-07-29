#ifndef CGEM_ATTRIBUTES_H
#define CGEM_ATTRIBUTES_H

#include "cgem/core/attribute.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_attributes cgem_attributes_t;

cgem_attributes_t *cgem_attributes_new(void);
void cgem_attributes_free(cgem_attributes_t *attributes);

bool cgem_attributes_add(cgem_attributes_t *attributes,
                         cgem_attribute_t *attribute);

size_t cgem_attributes_get_count(const cgem_attributes_t *attributes);
const cgem_attribute_t *cgem_attributes_get(
    const cgem_attributes_t *attributes, size_t index);
const cgem_attribute_t *cgem_attributes_find(
    const cgem_attributes_t *attributes, const char *key);
bool cgem_attributes_has(const cgem_attributes_t *attributes, const char *key);

#endif
