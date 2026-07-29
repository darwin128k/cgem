#ifndef CGEM_FILE_STREAM_H
#define CGEM_FILE_STREAM_H

#include "cgem/core/primitive.h"
#include "cgem/core/reader.h"
#include "cgem/core/writer.h"

cgem_reader_t *cgem_file_reader_new(const cgem_char_t *path);
cgem_writer_t *cgem_file_writer_new(const cgem_char_t *path);

#endif
