#include "cgem/core/memory_stream.h"

#include "cgem/core/allocator.h"

#include <string.h>

typedef struct {
    cgem_char_t *data;
    size_t size;
    size_t capacity;
} memory_buffer_t;

static memory_buffer_t *buffer_new(void)
{
    return cgem_alloc_zeroed(1, sizeof(memory_buffer_t));
}

static void buffer_free(memory_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }
    cgem_free(buffer->data);
    cgem_free(buffer);
}

static cgem_bool_t buffer_append(memory_buffer_t *buffer,
                                 const cgem_char_t *data, size_t size)
{
    if (buffer->size + size > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 64;
        cgem_char_t *grown;

        while (capacity < buffer->size + size) {
            capacity *= 2;
        }
        grown = cgem_realloc(buffer->data, capacity);
        if (!grown) {
            return false;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return true;
}

/* The writer always owns the buffer: its free() releases the data. */
static cgem_bool_t memory_write(void *self, const cgem_char_t *data,
                                size_t size)
{
    return buffer_append((memory_buffer_t *) self, data, size);
}

static void memory_writer_free(void *self)
{
    buffer_free((memory_buffer_t *) self);
}

static const cgem_writer_vtable_t memory_writer_vtable = {
    memory_write,
    memory_writer_free
};

typedef struct {
    memory_buffer_t *buffer;
    size_t at;
} memory_reader_state_t;

static cgem_bool_t memory_read(void *self, cgem_char_t *out, size_t capacity,
                               size_t *out_read)
{
    memory_reader_state_t *state = self;
    size_t available = state->buffer->size - state->at;
    size_t n = available < capacity ? available : capacity;

    if (n > 0) {
        memcpy(out, state->buffer->data + state->at, n);
        state->at += n;
    }
    *out_read = n;
    return true;
}

/* Reader paired with a writer (via a stream): borrows the buffer, the
 * writer owns and frees it. The writer must outlive this reader. */
static void memory_reader_free_borrowed(void *self)
{
    cgem_free(self);
}

static const cgem_reader_vtable_t borrowed_memory_reader_vtable = {
    memory_read,
    memory_reader_free_borrowed
};

/* Standalone reader (no paired writer): owns the buffer outright. */
static void memory_reader_free_owned(void *self)
{
    memory_reader_state_t *state = self;

    buffer_free(state->buffer);
    cgem_free(state);
}

static const cgem_reader_vtable_t owned_memory_reader_vtable = {
    memory_read,
    memory_reader_free_owned
};

static cgem_reader_t *reader_over_buffer(memory_buffer_t *buffer,
                                         const cgem_reader_vtable_t *vtable)
{
    memory_reader_state_t *state = cgem_alloc(sizeof(*state));
    cgem_reader_t *reader;

    if (!state) {
        return NULL;
    }
    state->buffer = buffer;
    state->at = 0;
    reader = cgem_reader_new(vtable, state);
    if (!reader) {
        cgem_free(state);
    }
    return reader;
}

cgem_stream_t *cgem_memory_stream_new(void)
{
    memory_buffer_t *buffer = buffer_new();
    cgem_writer_t *writer;
    cgem_reader_t *reader;
    cgem_stream_t *stream;

    if (!buffer) {
        return NULL;
    }
    writer = cgem_writer_new(&memory_writer_vtable, buffer);
    if (!writer) {
        buffer_free(buffer);
        return NULL;
    }
    reader = reader_over_buffer(buffer, &borrowed_memory_reader_vtable);
    if (!reader) {
        cgem_writer_free(writer); /* frees the buffer */
        return NULL;
    }
    stream = cgem_stream_new(reader, writer);
    if (!stream) {
        cgem_reader_free(reader);
        cgem_writer_free(writer);
        return NULL;
    }
    return stream;
}

cgem_writer_t *cgem_memory_writer_new(void)
{
    memory_buffer_t *buffer = buffer_new();
    cgem_writer_t *writer;

    if (!buffer) {
        return NULL;
    }
    writer = cgem_writer_new(&memory_writer_vtable, buffer);
    if (!writer) {
        buffer_free(buffer);
    }
    return writer;
}

cgem_reader_t *cgem_memory_reader_new(const cgem_char_t *data, size_t size)
{
    memory_buffer_t *buffer = buffer_new();
    cgem_reader_t *reader;

    if (!buffer) {
        return NULL;
    }
    if (size > 0 && !buffer_append(buffer, data, size)) {
        buffer_free(buffer);
        return NULL;
    }
    reader = reader_over_buffer(buffer, &owned_memory_reader_vtable);
    if (!reader) {
        buffer_free(buffer);
    }
    return reader;
}
