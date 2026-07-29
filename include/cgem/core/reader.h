#ifndef CGEM_READER_H
#define CGEM_READER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_reader cgem_reader_t;

typedef struct {
    bool (*read)(void *self, char *buffer, size_t capacity, size_t *out_read);
    void (*free)(void *self);
} cgem_reader_vtable_t;

cgem_reader_t *cgem_reader_new(const cgem_reader_vtable_t *vtable, void *self);
void cgem_reader_free(cgem_reader_t *reader);

bool cgem_reader_read(cgem_reader_t *reader, char *buffer, size_t capacity,
                      size_t *out_read);

#endif
