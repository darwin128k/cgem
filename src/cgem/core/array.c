#include "cgem/core/array.h"

#include "cgem/core/allocator.h"

#include <string.h>

bool cgem_array_init(cgem_array_t *array, size_t capacity, size_t element_size)
{
    array->data = NULL;
    array->size = 0;
    array->capacity = 0;
    array->element_size = element_size;
    if (capacity > 0 && !cgem_array_resize(array, capacity)) {
        return false;
    }
    return true;
}

void cgem_array_deinit(cgem_array_t *array)
{
    if (!array) {
        return;
    }
    cgem_free(array->data);
    array->data = NULL;
    array->size = 0;
    array->capacity = 0;
}

bool cgem_array_resize(cgem_array_t *array, size_t new_capacity)
{
    void *data;

    if (new_capacity <= array->capacity) {
        return true;
    }
    data = cgem_realloc(array->data, new_capacity * array->element_size);
    if (!data) {
        return false;
    }
    array->data = data;
    array->capacity = new_capacity;
    return true;
}

bool cgem_array_push_back(cgem_array_t *array, const void *element)
{
    if (!array || !element) {
        return false;
    }
    if (array->size == array->capacity) {
        size_t capacity = array->capacity ? array->capacity * 2 : 4;

        if (!cgem_array_resize(array, capacity)) {
            return false;
        }
    }
    memcpy((char *) array->data + array->size * array->element_size, element,
          array->element_size);
    array->size++;
    return true;
}

void cgem_array_pop_back(cgem_array_t *array)
{
    if (array && array->size > 0) {
        array->size--;
    }
}

void cgem_array_clear(cgem_array_t *array)
{
    if (array) {
        array->size = 0;
    }
}

void *cgem_array_at(const cgem_array_t *array, size_t index)
{
    if (!array || index >= array->size) {
        return NULL;
    }
    return (char *) array->data + index * array->element_size;
}

size_t cgem_array_size(const cgem_array_t *array)
{
    return array ? array->size : 0;
}

size_t cgem_array_capacity(const cgem_array_t *array)
{
    return array ? array->capacity : 0;
}
