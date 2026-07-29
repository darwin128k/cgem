#include "cgem/core/writer.h"

#include "cgem/core/allocator.h"

struct cgem_writer {
    const cgem_writer_vtable_t *vtable;
    void *self;
};

cgem_writer_t *cgem_writer_new(const cgem_writer_vtable_t *vtable, void *self)
{
    cgem_writer_t *writer;

    if (!vtable || !vtable->write) {
        return NULL;
    }
    writer = cgem_alloc(sizeof(*writer));
    if (!writer) {
        return NULL;
    }
    writer->vtable = vtable;
    writer->self = self;
    return writer;
}

void cgem_writer_free(cgem_writer_t *writer)
{
    if (!writer) {
        return;
    }
    if (writer->vtable->free) {
        writer->vtable->free(writer->self);
    }
    cgem_free(writer);
}

bool cgem_writer_write(cgem_writer_t *writer, const char *buffer, size_t size)
{
    if (!writer) {
        return false;
    }
    return writer->vtable->write(writer->self, buffer, size);
}
