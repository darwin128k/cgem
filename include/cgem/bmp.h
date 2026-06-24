#ifndef CGEM_BMP_H
#define CGEM_BMP_H

#include <stddef.h>

int cgem_bmp_preview(const char *path, int screen_rows, int screen_cols,
                     char *error, size_t error_size);

#endif
