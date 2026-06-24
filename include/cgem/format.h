#ifndef CGEM_FORMAT_H
#define CGEM_FORMAT_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

ssize_t cg_format_text_line(char *line, size_t length, size_t capacity);
int cg_format_stream(FILE *input, FILE *output);
int cg_format_path(const char *path, char *error, size_t error_size);

#endif
