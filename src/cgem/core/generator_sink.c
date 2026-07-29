#include "cgem/core/generator_sink.h"

#include "cgem/core/allocator.h"
#include "cgem/core/file_stream.h"
#include "cgem/platform.h"

#include <stdio.h>
#include <string.h>

struct cgem_generator_sink {
    cgem_char_t *root;
};

cgem_generator_sink_t *cgem_generator_sink_new(const cgem_char_t *root)
{
    cgem_generator_sink_t *sink;
    size_t length;

    if (!root) {
        return NULL;
    }
    sink = cgem_alloc(sizeof(*sink));
    if (!sink) {
        return NULL;
    }
    length = strlen(root);
    sink->root = cgem_alloc(length + 1);
    if (!sink->root) {
        cgem_free(sink);
        return NULL;
    }
    memcpy(sink->root, root, length + 1);
    return sink;
}

void cgem_generator_sink_free(cgem_generator_sink_t *sink)
{
    if (!sink) {
        return;
    }
    cgem_free(sink->root);
    cgem_free(sink);
}

static cgem_char_t *join_path(const cgem_char_t *root,
                              const cgem_char_t *relative)
{
    size_t root_length = strlen(root);
    cgem_bool_t needs_slash = root_length > 0 && root[root_length - 1] != '/';
    size_t relative_length = strlen(relative);
    cgem_char_t *joined = cgem_alloc(root_length + (needs_slash ? 1 : 0) +
                                     relative_length + 1);
    size_t at;

    if (!joined) {
        return NULL;
    }
    memcpy(joined, root, root_length);
    at = root_length;
    if (needs_slash) {
        joined[at++] = '/';
    }
    memcpy(joined + at, relative, relative_length + 1);
    return joined;
}

static cgem_bool_t ensure_parent_directory(const cgem_char_t *path,
                                           cgem_char_t *error,
                                           size_t error_size)
{
    const cgem_char_t *slash = strrchr(path, '/');
    cgem_char_t *dir;
    size_t dir_length;
    cgem_int_t result;

    if (!slash) {
        return true;
    }
    dir_length = (size_t) (slash - path);
    dir = cgem_alloc(dir_length + 1);
    if (!dir) {
        snprintf(error, error_size, "out of memory");
        return false;
    }
    memcpy(dir, path, dir_length);
    dir[dir_length] = '\0';
    result = platform_mkdir_p(dir, error, error_size);
    cgem_free(dir);
    return result == 0;
}

cgem_writer_t *cgem_generator_sink_open(cgem_generator_sink_t *sink,
                                       const cgem_char_t *relative_path,
                                       cgem_char_t *error, size_t error_size)
{
    cgem_char_t *full_path;
    cgem_writer_t *writer;

    if (!sink || !relative_path) {
        snprintf(error, error_size, "invalid sink or path");
        return NULL;
    }
    full_path = join_path(sink->root, relative_path);
    if (!full_path) {
        snprintf(error, error_size, "out of memory");
        return NULL;
    }
    if (!ensure_parent_directory(full_path, error, error_size)) {
        cgem_free(full_path);
        return NULL;
    }
    writer = cgem_file_writer_new(full_path);
    if (!writer) {
        snprintf(error, error_size, "%s: cannot open for writing", full_path);
    }
    cgem_free(full_path);
    return writer;
}
