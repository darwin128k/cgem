#ifndef CGEM_ARRAY_H
#define CGEM_ARRAY_H

#include "cgem/core/primitive.h"

#include <stddef.h>

typedef struct {
    void *data;
    size_t size;
    size_t capacity;
    size_t element_size;
} cgem_array_t;

cgem_bool_t cgem_array_init(cgem_array_t *array, size_t capacity,
                            size_t element_size);
void cgem_array_deinit(cgem_array_t *array);

cgem_bool_t cgem_array_resize(cgem_array_t *array, size_t new_capacity);
cgem_bool_t cgem_array_push_back(cgem_array_t *array, const void *element);
void cgem_array_pop_back(cgem_array_t *array);
void cgem_array_clear(cgem_array_t *array);

void *cgem_array_at(const cgem_array_t *array, size_t index);
size_t cgem_array_size(const cgem_array_t *array);
size_t cgem_array_capacity(const cgem_array_t *array);

#endif
