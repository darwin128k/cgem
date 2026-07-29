#include "cgem/core/allocator.h"

#include <stdlib.h>
#include <string.h>

static void *default_alloc(size_t size)
{
    return malloc(size);
}

static void *default_realloc(void *pointer, size_t size)
{
    return realloc(pointer, size);
}

static void default_free(void *pointer)
{
    free(pointer);
}

static cgem_alloc_fn_t current_alloc = default_alloc;
static cgem_realloc_fn_t current_realloc = default_realloc;
static cgem_free_fn_t current_free = default_free;

void cgem_allocator_set(cgem_alloc_fn_t alloc_fn, cgem_realloc_fn_t realloc_fn,
                        cgem_free_fn_t free_fn)
{
    current_alloc = alloc_fn ? alloc_fn : default_alloc;
    current_realloc = realloc_fn ? realloc_fn : default_realloc;
    current_free = free_fn ? free_fn : default_free;
}

void *cgem_alloc(size_t size)
{
    return current_alloc(size);
}

void *cgem_alloc_zeroed(size_t count, size_t size)
{
    size_t total = count * size;
    void *pointer = current_alloc(total);

    if (pointer) {
        memset(pointer, 0, total);
    }
    return pointer;
}

void *cgem_realloc(void *pointer, size_t size)
{
    return current_realloc(pointer, size);
}

void cgem_free(void *pointer)
{
    current_free(pointer);
}
