#ifndef CGEM_STREAM_H
#define CGEM_STREAM_H

#include "cgem/core/reader.h"
#include "cgem/core/writer.h"

typedef struct cgem_stream cgem_stream_t;

cgem_stream_t *cgem_stream_new(cgem_reader_t *reader, cgem_writer_t *writer);
void cgem_stream_free(cgem_stream_t *stream);

cgem_reader_t *cgem_stream_get_reader(cgem_stream_t *stream);
cgem_writer_t *cgem_stream_get_writer(cgem_stream_t *stream);

#endif
