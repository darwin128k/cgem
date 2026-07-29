#ifndef CGEM_ALLOCATOR_H
#define CGEM_ALLOCATOR_H

#include <stddef.h>

typedef void *(*cgem_alloc_fn_t)(size_t size);
typedef void *(*cgem_realloc_fn_t)(void *pointer, size_t size);
typedef void (*cgem_free_fn_t)(void *pointer);

void cgem_allocator_set(cgem_alloc_fn_t alloc_fn, cgem_realloc_fn_t realloc_fn,
                        cgem_free_fn_t free_fn);

void *cgem_alloc(size_t size);
void *cgem_alloc_zeroed(size_t count, size_t size);
void *cgem_realloc(void *pointer, size_t size);
void cgem_free(void *pointer);

#endif
