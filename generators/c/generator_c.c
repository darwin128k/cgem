#include "cgem/generator_abi.h"

#include "cgem/core/allocator.h"
#include "cgem/core/attribute.h"
#include "cgem/core/attributes.h"
#include "cgem/core/node.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *node_string_attribute(cgem_node_t *node, const char *key)
{
    const cgem_attribute_t *attribute =
        cgem_attributes_find(cgem_node_get_attributes(node), key);

    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
}

static void write_guard_name(const char *name, char *out, size_t out_size)
{
    size_t i;
    size_t length = strlen(name);

    if (length > out_size - 3) {
        length = out_size - 3;
    }
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char) name[i];

        out[i] = (char) (isalnum(ch) ? toupper(ch) : '_');
    }
    out[length] = '_';
    out[length + 1] = 'H';
    out[length + 2] = '\0';
}

static bool write_module_header(cgem_generator_sink_t *sink,
                                const char *path, const char *name,
                                char *error, size_t error_size)
{
    char relative_path[512];
    char guard[128];
    char content[512];
    cgem_writer_t *writer;
    bool ok;

    snprintf(relative_path, sizeof(relative_path), "%s%s%s.h", path,
             path[0] ? "/" : "", name);
    write_guard_name(name, guard, sizeof(guard));
    writer = cgem_generator_sink_open(sink, relative_path, error, error_size);
    if (!writer) {
        return false;
    }
    snprintf(content, sizeof(content),
             "#ifndef %s\n#define %s\n\n#endif\n", guard, guard);
    ok = cgem_writer_write(writer, content, strlen(content));
    if (!ok) {
        snprintf(error, error_size, "%s: write failed", relative_path);
    }
    cgem_writer_free(writer);
    return ok;
}

static bool walk(cgem_node_t *node, cgem_generator_sink_t *sink,
                 const char *path, char *error, size_t error_size)
{
    const char *type = node_string_attribute(node, "type");
    const char *name = node_string_attribute(node, "name");
    char *child_path = NULL;
    const char *next_path = path;
    size_t count;
    size_t i;
    bool ok = true;

    if (type && name && strcmp(type, "module") == 0) {
        return write_module_header(sink, path, name, error, error_size);
    }
    if (name) {
        size_t path_length = strlen(path);
        size_t name_length = strlen(name);

        child_path = cgem_alloc(path_length + 1 + name_length + 1);
        if (!child_path) {
            snprintf(error, error_size, "out of memory");
            return false;
        }
        if (path_length) {
            memcpy(child_path, path, path_length);
            child_path[path_length] = '/';
            memcpy(child_path + path_length + 1, name, name_length + 1);
        } else {
            memcpy(child_path, name, name_length + 1);
        }
        next_path = child_path;
    }
    count = cgem_node_get_count(node);
    for (i = 0; i < count && ok; i++) {
        cgem_node_t *child = cgem_node_get(node, i);

        ok = walk(child, sink, next_path, error, error_size);
    }
    cgem_free(child_path);
    return ok;
}

static bool generate_default(cgem_node_t *root, cgem_generator_sink_t *sink,
                             char *error, size_t error_size)
{
    if (!root || !sink) {
        snprintf(error, error_size, "c generator: missing root or sink");
        return false;
    }
    return walk(root, sink, "", error, error_size);
}

static bool init(cgem_generator_registrar_t *registrar, char *error,
                 size_t error_size)
{
    if (!cgem_generator_registrar_add_attribute_key(registrar, "name") ||
        !cgem_generator_registrar_add_attribute_key(registrar, "type") ||
        !cgem_generator_registrar_add_attribute_key(registrar, "define") ||
        !cgem_generator_registrar_add_target(registrar, "default",
                                             generate_default)) {
        snprintf(error, error_size, "c generator: registration failed");
        return false;
    }
    return true;
}

static void deinit(void)
{
}

static const cgem_generator_vtable_t vtable = {
    CGEM_GENERATOR_ABI_VERSION,
    "c",
    init,
    deinit
};

const cgem_generator_vtable_t *cgem_generator_get_vtable(void)
{
    return &vtable;
}
