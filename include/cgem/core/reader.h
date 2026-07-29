#ifndef CGEM_READER_H
#define CGEM_READER_H

#include "cgem/core/primitive.h"

#include <stddef.h>

typedef struct cgem_reader cgem_reader_t;

typedef struct {
    cgem_bool_t (*read)(void *self, cgem_char_t *buffer, size_t capacity,
                        size_t *out_read);
    void (*free)(void *self);
} cgem_reader_vtable_t;

cgem_reader_t *cgem_reader_new(const cgem_reader_vtable_t *vtable, void *self);
void cgem_reader_free(cgem_reader_t *reader);

cgem_bool_t cgem_reader_read(cgem_reader_t *reader, cgem_char_t *buffer,
                             size_t capacity, size_t *out_read);

#endif
