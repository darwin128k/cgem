#include "cgem/core/generator.h"

#include "cgem/core/allocator.h"
#include "cgem/platform.h"

#include <stdio.h>
#include <string.h>

struct cgem_generator {
    void *library;
    const cgem_generator_vtable_t *vtable;
};

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
    if (!vtable->generate) {
        snprintf(error, error_size, "%s: generator has no generate function",
                 path);
        platform_library_close(library);
        return NULL;
    }
    if (vtable->init && !vtable->init(error, error_size)) {
        platform_library_close(library);
        return NULL;
    }
    generator = cgem_alloc(sizeof(*generator));
    if (!generator) {
        if (vtable->deinit) {
            vtable->deinit();
        }
        snprintf(error, error_size, "out of memory");
        platform_library_close(library);
        return NULL;
    }
    generator->library = library;
    generator->vtable = vtable;
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
    for (i = 0; i < generator->vtable->known_attribute_key_count; i++) {
        if (strcmp(generator->vtable->known_attribute_keys[i], key) == 0) {
            return true;
        }
    }
    return false;
}

bool cgem_generator_generate(cgem_generator_t *generator, cgem_node_t *root,
                             cgem_writer_t *writer, char *error,
                             size_t error_size)
{
    if (!generator) {
        snprintf(error, error_size, "no generator loaded");
        return false;
    }
    return generator->vtable->generate(root, writer, error, error_size);
}
