#ifndef CGEM_MEMORY_STREAM_H
#define CGEM_MEMORY_STREAM_H

#include "cgem/core/reader.h"
#include "cgem/core/stream.h"
#include "cgem/core/writer.h"

cgem_stream_t *cgem_memory_stream_new(void);
cgem_writer_t *cgem_memory_writer_new(void);
cgem_reader_t *cgem_memory_reader_new(const char *data, size_t size);

#endif
