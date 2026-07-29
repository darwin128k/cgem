#include "cgem/core/generator.h"

#include "cgem/core/allocator.h"
#include "cgem/core/array.h"
#include "cgem/platform.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    char *name;
    cgem_generate_fn_t fn;
} generator_target_t;

struct cgem_generator_registrar {
    cgem_array_t attribute_keys;
    cgem_array_t targets;
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
    copy = copy_string(key);
    if (!copy) {
        return false;
    }
    if (!cgem_array_push_back(&registrar->attribute_keys, &copy)) {
        cgem_free(copy);
        return false;
    }
    return true;
}

bool cgem_generator_registrar_add_target(cgem_generator_registrar_t *registrar,
                                         const char *name,
                                         cgem_generate_fn_t fn)
{
    generator_target_t target;

    if (!registrar || !name || !fn) {
        return false;
    }
    target.name = copy_string(name);
    if (!target.name) {
        return false;
    }
    target.fn = fn;
    if (!cgem_array_push_back(&registrar->targets, &target)) {
        cgem_free(target.name);
        return false;
    }
    return true;
}

static void init_registrar(cgem_generator_registrar_t *registrar)
{
    cgem_array_init(&registrar->attribute_keys, 0, sizeof(char *));
    cgem_array_init(&registrar->targets, 0, sizeof(generator_target_t));
}

static void free_registrar_contents(cgem_generator_registrar_t *registrar)
{
    size_t i;

    for (i = 0; i < cgem_array_size(&registrar->attribute_keys); i++) {
        cgem_free(*(char **) cgem_array_at(&registrar->attribute_keys, i));
    }
    cgem_array_deinit(&registrar->attribute_keys);
    for (i = 0; i < cgem_array_size(&registrar->targets); i++) {
        generator_target_t *target = cgem_array_at(&registrar->targets, i);

        cgem_free(target->name);
    }
    cgem_array_deinit(&registrar->targets);
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
    generator = cgem_alloc(sizeof(*generator));
    if (!generator) {
        snprintf(error, error_size, "out of memory");
        platform_library_close(library);
        return NULL;
    }
    generator->library = library;
    generator->vtable = vtable;
    init_registrar(&generator->registrar);
    if (!vtable->init(&generator->registrar, error, error_size)) {
        free_registrar_contents(&generator->registrar);
        cgem_free(generator);
        platform_library_close(library);
        return NULL;
    }
    if (cgem_array_size(&generator->registrar.targets) == 0) {
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
    for (i = 0; i < cgem_array_size(&generator->registrar.attribute_keys); i++) {
        char *existing = *(char **) cgem_array_at(&generator->registrar.attribute_keys, i);

        if (strcmp(existing, key) == 0) {
            return true;
        }
    }
    return false;
}

size_t cgem_generator_get_attribute_key_count(const cgem_generator_t *generator)
{
    return generator ? cgem_array_size(&generator->registrar.attribute_keys) : 0;
}

const char *cgem_generator_get_attribute_key(const cgem_generator_t *generator,
                                             size_t index)
{
    char **slot;

    if (!generator) {
        return NULL;
    }
    slot = cgem_array_at(&generator->registrar.attribute_keys, index);
    return slot ? *slot : NULL;
}

size_t cgem_generator_get_target_count(const cgem_generator_t *generator)
{
    return generator ? cgem_array_size(&generator->registrar.targets) : 0;
}

const char *cgem_generator_get_target_name(const cgem_generator_t *generator,
                                           size_t index)
{
    generator_target_t *target;

    if (!generator) {
        return NULL;
    }
    target = cgem_array_at(&generator->registrar.targets, index);
    return target ? target->name : NULL;
}

bool cgem_generator_generate(cgem_generator_t *generator, const char *target,
                             cgem_node_t *root, cgem_generator_sink_t *sink,
                             char *error, size_t error_size)
{
    size_t i;

    if (!generator || !target) {
        snprintf(error, error_size, "no generator or target specified");
        return false;
    }
    for (i = 0; i < cgem_array_size(&generator->registrar.targets); i++) {
        generator_target_t *candidate =
            cgem_array_at(&generator->registrar.targets, i);

        if (strcmp(candidate->name, target) == 0) {
            return candidate->fn(root, sink, error, error_size);
        }
    }
    snprintf(error, error_size, "unknown generator target: %s", target);
    return false;
}
