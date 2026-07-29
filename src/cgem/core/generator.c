#include "cgem/core/generator.h"

#include "cgem/core/allocator.h"
#include "cgem/platform.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char *name;
    cgem_generate_fn_t fn;
} generator_target_t;

struct cgem_generator_registrar {
    char **attribute_keys;
    size_t attribute_key_count;
    size_t attribute_key_capacity;

    generator_target_t *targets;
    size_t target_count;
    size_t target_capacity;
};

struct cgem_generator {
    void *library;
    const cgem_generator_vtable_t *vtable;
    cgem_generator_registrar_t registrar;
};

static char *copy_string(const char *text)
{
    size_t length = strlen(text);
    char *copy = cgem_alloc(length + 1);

    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

bool cgem_generator_registrar_add_attribute_key(
    cgem_generator_registrar_t *registrar, const char *key)
{
    char *copy;

    if (!registrar || !key) {
        return false;
    }
    if (registrar->attribute_key_count == registrar->attribute_key_capacity) {
        size_t capacity = registrar->attribute_key_capacity
                               ? registrar->attribute_key_capacity * 2
                               : 4;
        char **keys = cgem_realloc(registrar->attribute_keys,
                                   capacity * sizeof(*keys));

        if (!keys) {
            return false;
        }
        registrar->attribute_keys = keys;
        registrar->attribute_key_capacity = capacity;
    }
    copy = copy_string(key);
    if (!copy) {
        return false;
    }
    registrar->attribute_keys[registrar->attribute_key_count++] = copy;
    return true;
}

bool cgem_generator_registrar_add_target(cgem_generator_registrar_t *registrar,
                                         const char *name,
                                         cgem_generate_fn_t fn)
{
    char *copy;

    if (!registrar || !name || !fn) {
        return false;
    }
    if (registrar->target_count == registrar->target_capacity) {
        size_t capacity =
            registrar->target_capacity ? registrar->target_capacity * 2 : 4;
        generator_target_t *targets = cgem_realloc(
            registrar->targets, capacity * sizeof(*targets));

        if (!targets) {
            return false;
        }
        registrar->targets = targets;
        registrar->target_capacity = capacity;
    }
    copy = copy_string(name);
    if (!copy) {
        return false;
    }
    registrar->targets[registrar->target_count].name = copy;
    registrar->targets[registrar->target_count].fn = fn;
    registrar->target_count++;
    return true;
}

static void free_registrar_contents(cgem_generator_registrar_t *registrar)
{
    size_t i;

    for (i = 0; i < registrar->attribute_key_count; i++) {
        cgem_free(registrar->attribute_keys[i]);
    }
    cgem_free(registrar->attribute_keys);
    for (i = 0; i < registrar->target_count; i++) {
        cgem_free(registrar->targets[i].name);
    }
    cgem_free(registrar->targets);
}

cgem_generator_t *cgem_generator_load(const char *path, char *error,
                                      size_t error_size)
{
    void *library;
    cgem_generator_entry_fn_t entry;
    const cgem_generator_vtable_t *vtable;
    cgem_generator_t *generator;

    library = platform_library_open(path);
    if (!library) {
        snprintf(error, error_size, "%s: cannot open generator library", path);
        return NULL;
    }
    entry = (cgem_generator_entry_fn_t)
        platform_library_symbol(library, CGEM_GENERATOR_ENTRY_SYMBOL);
    if (!entry) {
        snprintf(error, error_size, "%s: missing %s entry point", path,
                 CGEM_GENERATOR_ENTRY_SYMBOL);
        platform_library_close(library);
        return NULL;
    }
    vtable = entry();
    if (!vtable) {
        snprintf(error, error_size, "%s: generator returned no vtable", path);
        platform_library_close(library);
        return NULL;
    }
    if (vtable->abi_version != CGEM_GENERATOR_ABI_VERSION) {
        snprintf(error, error_size,
                 "%s: generator ABI version %u does not match expected %u",
                 path, vtable->abi_version, CGEM_GENERATOR_ABI_VERSION);
        platform_library_close(library);
        return NULL;
    }
    if (!vtable->init) {
        snprintf(error, error_size, "%s: generator has no init function", path);
        platform_library_close(library);
        return NULL;
    }
    generator = cgem_alloc_zeroed(1, sizeof(*generator));
    if (!generator) {
        snprintf(error, error_size, "out of memory");
        platform_library_close(library);
        return NULL;
    }
    generator->library = library;
    generator->vtable = vtable;
    if (!vtable->init(&generator->registrar, error, error_size)) {
        free_registrar_contents(&generator->registrar);
        cgem_free(generator);
        platform_library_close(library);
        return NULL;
    }
    if (generator->registrar.target_count == 0) {
        snprintf(error, error_size, "%s: generator registered no targets", path);
        if (vtable->deinit) {
            vtable->deinit();
        }
        free_registrar_contents(&generator->registrar);
        cgem_free(generator);
        platform_library_close(library);
        return NULL;
    }
    return generator;
}

void cgem_generator_free(cgem_generator_t *generator)
{
    if (!generator) {
        return;
    }
    if (generator->vtable->deinit) {
        generator->vtable->deinit();
    }
    free_registrar_contents(&generator->registrar);
    platform_library_close(generator->library);
    cgem_free(generator);
}

const char *cgem_generator_get_name(const cgem_generator_t *generator)
{
    return generator ? generator->vtable->name : NULL;
}

bool cgem_generator_has_attribute_key(const cgem_generator_t *generator,
                                      const char *key)
{
    size_t i;

    if (!generator || !key) {
        return false;
    }
    for (i = 0; i < generator->registrar.attribute_key_count; i++) {
        if (strcmp(generator->registrar.attribute_keys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

size_t cgem_generator_get_attribute_key_count(const cgem_generator_t *generator)
{
    return generator ? generator->registrar.attribute_key_count : 0;
}

const char *cgem_generator_get_attribute_key(const cgem_generator_t *generator,
                                             size_t index)
{
    if (!generator || index >= generator->registrar.attribute_key_count) {
        return NULL;
    }
    return generator->registrar.attribute_keys[index];
}

size_t cgem_generator_get_target_count(const cgem_generator_t *generator)
{
    return generator ? generator->registrar.target_count : 0;
}

const char *cgem_generator_get_target_name(const cgem_generator_t *generator,
                                           size_t index)
{
    if (!generator || index >= generator->registrar.target_count) {
        return NULL;
    }
    return generator->registrar.targets[index].name;
}

bool cgem_generator_generate(cgem_generator_t *generator, const char *target,
                             cgem_node_t *root, cgem_writer_t *writer,
                             char *error, size_t error_size)
{
    size_t i;

    if (!generator || !target) {
        snprintf(error, error_size, "no generator or target specified");
        return false;
    }
    for (i = 0; i < generator->registrar.target_count; i++) {
        if (strcmp(generator->registrar.targets[i].name, target) == 0) {
            return generator->registrar.targets[i].fn(root, writer, error,
                                                       error_size);
        }
    }
    snprintf(error, error_size, "unknown generator target: %s", target);
    return false;
}
