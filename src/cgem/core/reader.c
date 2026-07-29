#include "cgem/core/reader.h"

#include <stdlib.h>

struct cgem_reader {
    const cgem_reader_vtable_t *vtable;
    void *self;
};

cgem_reader_t *cgem_reader_new(const cgem_reader_vtable_t *vtable, void *self)
{
    cgem_reader_t *reader;

    if (!vtable || !vtable->read) {
        return NULL;
    }
    reader = malloc(sizeof(*reader));
    if (!reader) {
        return NULL;
    }
    reader->vtable = vtable;
    reader->self = self;
    return reader;
}

void cgem_reader_free(cgem_reader_t *reader)
{
    if (!reader) {
        return;
    }
    if (reader->vtable->free) {
        reader->vtable->free(reader->self);
    }
    free(reader);
}

bool cgem_reader_read(cgem_reader_t *reader, char *buffer, size_t capacity,
                      size_t *out_read)
{
    if (!reader) {
        return false;
    }
    return reader->vtable->read(reader->self, buffer, capacity, out_read);
}
