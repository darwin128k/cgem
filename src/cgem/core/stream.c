#include "cgem/core/stream.h"

#include "cgem/core/allocator.h"

struct cgem_stream {
    cgem_reader_t *reader;
    cgem_writer_t *writer;
};

cgem_stream_t *cgem_stream_new(cgem_reader_t *reader, cgem_writer_t *writer)
{
    cgem_stream_t *stream = cgem_alloc(sizeof(*stream));

    if (!stream) {
        return NULL;
    }
    stream->reader = reader;
    stream->writer = writer;
    return stream;
}

void cgem_stream_free(cgem_stream_t *stream)
{
    if (!stream) {
        return;
    }
    cgem_reader_free(stream->reader);
    cgem_writer_free(stream->writer);
    cgem_free(stream);
}

cgem_reader_t *cgem_stream_get_reader(cgem_stream_t *stream)
{
    return stream ? stream->reader : NULL;
}

cgem_writer_t *cgem_stream_get_writer(cgem_stream_t *stream)
{
    return stream ? stream->writer : NULL;
}
