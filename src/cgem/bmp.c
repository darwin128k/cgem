#include "cgem/bmp.h"

#include "cgem/platform.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} PreviewBuffer;

static uint16_t read_u16(const unsigned char *data)
{
    return (uint16_t) data[0] | ((uint16_t) data[1] << 8);
}

static uint32_t read_u32(const unsigned char *data)
{
    return (uint32_t) data[0] | ((uint32_t) data[1] << 8) |
           ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int buffer_append(PreviewBuffer *buffer, const char *text, size_t length)
{
    if (length > SIZE_MAX - buffer->length - 1) {
        return -1;
    }
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        char *grown;

        while (capacity < buffer->length + length + 1) {
            if (capacity > SIZE_MAX / 2) {
                return -1;
            }
            capacity *= 2;
        }
        grown = realloc(buffer->data, capacity);
        if (!grown) {
            return -1;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 0;
}

static int buffer_printf(PreviewBuffer *buffer, const char *format,
                         int a, int b, int c, int d, int e)
{
    char text[96];
    int length = snprintf(text, sizeof(text), format, a, b, c, d, e);

    if (length < 0 || (size_t) length >= sizeof(text)) {
        return -1;
    }
    return buffer_append(buffer, text, (size_t) length);
}

int cgem_bmp_preview(const char *path, int screen_rows, int screen_cols,
                     char *error, size_t error_size)
{
    unsigned char header[54];
    unsigned char *pixels = NULL;
    FILE *input = fopen(path, "rb");
    uint32_t pixel_offset;
    int32_t width;
    int32_t signed_height;
    size_t height;
    uint16_t bits;
    size_t bytes_per_pixel;
    size_t row_stride;
    size_t data_size;
    int target_width;
    int target_height;
    int start_col;
    int start_row;
    PreviewBuffer output = {0};
    int result = -1;

    if (!input) {
        snprintf(error, error_size, "cannot open BMP: %s", path);
        return -1;
    }
    if (fread(header, 1, sizeof(header), input) != sizeof(header) ||
        header[0] != 'B' || header[1] != 'M' || read_u32(header + 14) < 40) {
        snprintf(error, error_size, "invalid BMP file: %s", path);
        goto done;
    }
    pixel_offset = read_u32(header + 10);
    width = (int32_t) read_u32(header + 18);
    signed_height = (int32_t) read_u32(header + 22);
    bits = read_u16(header + 28);
    if (pixel_offset < sizeof(header) ||
#if LONG_MAX < UINT32_MAX
        pixel_offset > (uint32_t) LONG_MAX ||
#endif
        width <= 0 || signed_height == 0 || signed_height == INT32_MIN ||
        read_u16(header + 26) != 1 || read_u32(header + 30) != 0 ||
        (bits != 24 && bits != 32)) {
        snprintf(error, error_size,
                 "BMP must be uncompressed 24-bit or 32-bit: %s", path);
        goto done;
    }
    height = signed_height < 0 ? (size_t) -(int64_t) signed_height
                               : (size_t) signed_height;
    bytes_per_pixel = bits / 8;
    if ((size_t) width > (SIZE_MAX - 3) / bytes_per_pixel) {
        snprintf(error, error_size, "BMP is too large: %s", path);
        goto done;
    }
    row_stride = ((size_t) width * bytes_per_pixel + 3) & ~(size_t) 3;
    if (height > SIZE_MAX / row_stride) {
        snprintf(error, error_size, "BMP is too large: %s", path);
        goto done;
    }
    data_size = row_stride * height;
    pixels = malloc(data_size);
    if (!pixels) {
        snprintf(error, error_size, "out of memory");
        goto done;
    }
    if (fseek(input, (long) pixel_offset, SEEK_SET) != 0 ||
        fread(pixels, 1, data_size, input) != data_size) {
        snprintf(error, error_size, "truncated BMP file: %s", path);
        goto done;
    }

    target_height = screen_rows - 3;
    target_width = screen_cols - 2;
    if (target_height < 1 || target_width < 1) {
        snprintf(error, error_size, "terminal is too small for BMP preview");
        goto done;
    }
    {
        int64_t fitted_width = ((int64_t) width * target_height * 2) /
                               (int64_t) height;

        if (fitted_width < target_width) {
            target_width = fitted_width > 0 ? (int) fitted_width : 1;
        } else {
            target_height = (int) (((int64_t) height * target_width) /
                                   ((int64_t) width * 2));
            if (target_height < 1) {
                target_height = 1;
            }
        }
    }
    start_col = (screen_cols - target_width) / 2 + 1;
    start_row = (screen_rows - target_height - 1) / 2 + 1;
    if (buffer_append(&output, "\x1b[0m\x1b[2J\x1b[?25l",
                      sizeof("\x1b[0m\x1b[2J\x1b[?25l") - 1) != 0) {
        goto memory_error;
    }
    for (int y = 0; y < target_height; y++) {
        size_t source_y = (size_t) y * height / (size_t) target_height;
        size_t stored_y = signed_height > 0 ? height - source_y - 1 : source_y;
        const unsigned char *row = pixels + stored_y * row_stride;

        if (buffer_printf(&output, "\x1b[%d;%dH", start_row + y, start_col,
                          0, 0, 0) != 0) {
            goto memory_error;
        }
        for (int x = 0; x < target_width; x++) {
            size_t source_x = (size_t) x * (size_t) width /
                              (size_t) target_width;
            const unsigned char *pixel = row + source_x * bytes_per_pixel;

            if (buffer_printf(&output, "\x1b[48;2;%d;%d;%dm ",
                              pixel[2], pixel[1], pixel[0], 0, 0) != 0) {
                goto memory_error;
            }
        }
        if (buffer_append(&output, "\x1b[0m", 4) != 0) {
            goto memory_error;
        }
    }
    if (buffer_printf(&output, "\x1b[%d;%dH\x1b[0mBMP preview - press any key",
                      screen_rows, 1, 0, 0, 0) != 0) {
        goto memory_error;
    }
    platform_terminal_write(output.data, output.length);
    result = 0;
    goto done;

memory_error:
    snprintf(error, error_size, "out of memory");
done:
    free(output.data);
    free(pixels);
    fclose(input);
    return result;
}
