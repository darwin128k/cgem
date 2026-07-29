#ifndef CGEM_WRITER_H
#define CGEM_WRITER_H

#include <stdbool.h>
#include <stddef.h>

typedef struct cgem_writer cgem_writer_t;

typedef struct {
    bool (*write)(void *self, const char *buffer, size_t size);
    void (*free)(void *self);
} cgem_writer_vtable_t;

cgem_writer_t *cgem_writer_new(const cgem_writer_vtable_t *vtable, void *self);
void cgem_writer_free(cgem_writer_t *writer);

bool cgem_writer_write(cgem_writer_t *writer, const char *buffer, size_t size);

#endif
