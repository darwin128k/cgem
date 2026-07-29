#include "cgem/core/file_stream.h"

#include <stdio.h>

static cgem_bool_t file_read(void *self, cgem_char_t *buffer, size_t capacity,
                             size_t *out_read)
{
    FILE *file = self;
    size_t n = fread(buffer, 1, capacity, file);

    if (n == 0 && ferror(file)) {
        return false;
    }
    *out_read = n;
    return true;
}

static void file_reader_free(void *self)
{
    fclose((FILE *) self);
}

static const cgem_reader_vtable_t file_reader_vtable = {
    file_read,
    file_reader_free
};

cgem_reader_t *cgem_file_reader_new(const cgem_char_t *path)
{
    FILE *file = fopen(path, "rb");
    cgem_reader_t *reader;

    if (!file) {
        return NULL;
    }
    reader = cgem_reader_new(&file_reader_vtable, file);
    if (!reader) {
        fclose(file);
    }
    return reader;
}

static cgem_bool_t file_write(void *self, const cgem_char_t *data, size_t size)
{
    FILE *file = self;

    return fwrite(data, 1, size, file) == size;
}

static void file_writer_free(void *self)
{
    fclose((FILE *) self);
}

static const cgem_writer_vtable_t file_writer_vtable = {
    file_write,
    file_writer_free
};

cgem_writer_t *cgem_file_writer_new(const cgem_char_t *path)
{
    FILE *file = fopen(path, "wb");
    cgem_writer_t *writer;

    if (!file) {
        return NULL;
    }
    writer = cgem_writer_new(&file_writer_vtable, file);
    if (!writer) {
        fclose(file);
    }
    return writer;
}
