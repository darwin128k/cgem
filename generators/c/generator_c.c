#define _POSIX_C_SOURCE 200809L

#include "cgem/generator_abi.h"

#include "cgem/core/allocator.h"
#include "cgem/core/attribute.h"
#include "cgem/core/attributes.h"
#include "cgem/core/node.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const cgem_char_t *node_string_attribute(cgem_node_t *node,
                                                 const cgem_char_t *key)
{
    const cgem_attribute_t *attribute =
        cgem_attributes_find(cgem_node_get_attributes(node), key);

    if (!attribute) {
        return NULL;
    }
    return cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
}

static void write_guard_name(const cgem_char_t *name, cgem_char_t *out,
                             size_t out_size)
{
    size_t i;
    size_t length = strlen(name);

    if (length > out_size - 3) {
        length = out_size - 3;
    }
    for (i = 0; i < length; i++) {
        cgem_uchar_t ch = (cgem_uchar_t) name[i];

        out[i] = (cgem_char_t) (isalnum(ch) ? toupper(ch) : '_');
    }
    out[length] = '_';
    out[length + 1] = 'H';
    out[length + 2] = '\0';
}

static cgem_bool_t write_module_header(cgem_generator_sink_t *sink,
                                       const cgem_char_t *path,
                                       const cgem_char_t *name,
                                       cgem_char_t *error, size_t error_size)
{
    cgem_char_t relative_path[512];
    cgem_char_t guard[128];
    cgem_char_t content[512];
    cgem_writer_t *writer;
    cgem_bool_t ok;

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

static cgem_bool_t walk(cgem_node_t *node, cgem_generator_sink_t *sink,
                        const cgem_char_t *path, cgem_char_t *error,
                        size_t error_size)
{
    const cgem_char_t *type = node_string_attribute(node, "type");
    const cgem_char_t *name = node_string_attribute(node, "name");
    cgem_char_t *child_path = NULL;
    const cgem_char_t *next_path = path;
    size_t count;
    size_t i;
    cgem_bool_t ok = true;

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

static cgem_bool_t generate_default(cgem_node_t *root,
                                    cgem_generator_sink_t *sink,
                                    cgem_char_t *error, size_t error_size)
{
    if (!root || !sink) {
        snprintf(error, error_size, "c generator: missing root or sink");
        return false;
    }
    return walk(root, sink, "", error, error_size);
}

/* Queries the actually configured compiler (never the host compiler that
 * happens to be running cgem) for a predefined sizeof-macro, e.g.
 * "__SIZEOF_INT__". This is what keeps type sizes correct under cross
 * compilation: the compiler binary itself knows its target's ABI, so
 * asking it is the only reproducible source of truth -- cgem_core never
 * hardcodes a size. */
static cgem_bool_t query_sizeof_macro(const cgem_char_t *compiler,
                                      const cgem_char_t *macro, size_t *out,
                                      cgem_char_t *error, size_t error_size)
{
    cgem_char_t command[256];
    cgem_char_t line[256];
    FILE *pipe;
    cgem_bool_t found = false;

    snprintf(command, sizeof(command), "%s -E -dM -x c /dev/null 2>/dev/null",
             compiler);
    pipe = popen(command, "r");
    if (!pipe) {
        snprintf(error, error_size,
                 "c generator: failed to run '%s' to query type sizes",
                 compiler);
        return false;
    }
    while (fgets(line, sizeof(line), pipe)) {
        cgem_char_t name[128];
        long value;

        if (sscanf(line, "#define %127s %ld", name, &value) == 2 &&
            strcmp(name, macro) == 0) {
            *out = (size_t) value;
            found = true;
            break;
        }
    }
    pclose(pipe);
    if (!found) {
        snprintf(error, error_size,
                 "c generator: compiler '%s' did not report %s -- cannot "
                 "determine the real target size",
                 compiler, macro);
        return false;
    }
    return true;
}

static cgem_bool_t register_sized_type(cgem_generator_registrar_t *registrar,
                                       const cgem_char_t *compiler,
                                       const cgem_char_t *type_name,
                                       const cgem_char_t *sizeof_macro,
                                       cgem_char_t *error, size_t error_size)
{
    size_t size;

    if (!query_sizeof_macro(compiler, sizeof_macro, &size, error,
                            error_size)) {
        return false;
    }
    return cgem_generator_registrar_add_type_key(registrar, type_name, size);
}

static const cgem_char_t *resolve_compiler(const cgem_attributes_t *config)
{
    const cgem_attribute_t *attribute;
    const cgem_char_t *compiler;

    if (!config) {
        return "cc";
    }
    attribute = cgem_attributes_find(config, "c.compiler");
    if (!attribute) {
        return "cc";
    }
    compiler = cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
    return (compiler && *compiler) ? compiler : "cc";
}

static cgem_bool_t standard_at_least_c99(const cgem_attributes_t *config)
{
    const cgem_attribute_t *attribute;
    const cgem_char_t *standard;

    if (!config) {
        /* no config: assume a modern-enough compiler, same as if the
         * caller had not restricted the standard at all */
        return true;
    }
    attribute = cgem_attributes_find(config, "c.standard");
    if (!attribute) {
        return true;
    }
    standard = cgem_attribute_value_get_string(cgem_attribute_get_value(attribute));
    if (!standard) {
        return true;
    }
    return strcmp(standard, "c89") != 0 && strcmp(standard, "c90") != 0 &&
           strcmp(standard, "ansi") != 0;
}

static cgem_bool_t init(cgem_generator_registrar_t *registrar,
                        const cgem_attributes_t *config, cgem_char_t *error,
                        size_t error_size)
{
    const cgem_char_t *compiler = resolve_compiler(config);

    if (!cgem_generator_registrar_add_attribute_key(registrar, "name") ||
        !cgem_generator_registrar_add_attribute_key(registrar, "type") ||
        !cgem_generator_registrar_add_attribute_key(registrar, "define")) {
        snprintf(error, error_size, "c generator: registration failed");
        return false;
    }
    /* sizeof(char) is mandated to be 1 by the C standard itself -- not an
     * implementation trait, so unlike the others it needs no toolchain
     * probe. */
    if (!cgem_generator_registrar_add_type_key(registrar, "char", 1)) {
        snprintf(error, error_size, "c generator: registration failed");
        return false;
    }
    if (!register_sized_type(registrar, compiler, "short", "__SIZEOF_SHORT__",
                             error, error_size) ||
        !register_sized_type(registrar, compiler, "int", "__SIZEOF_INT__",
                             error, error_size) ||
        !register_sized_type(registrar, compiler, "long", "__SIZEOF_LONG__",
                             error, error_size)) {
        return false;
    }
    /* long long only exists from C99 onward */
    if (standard_at_least_c99(config) &&
        !register_sized_type(registrar, compiler, "long_long",
                             "__SIZEOF_LONG_LONG__", error, error_size)) {
        return false;
    }
    if (!cgem_generator_registrar_add_target(registrar, "default",
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
