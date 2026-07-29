#ifndef CGEM_REF_H
#define CGEM_REF_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

/* A reference expression: looks up a dotted path (e.g. "self.value",
 * resolved the same way cgem_resolve walks any other path). A true leaf --
 * referencing something never itself has sub-expressions. The path is
 * carried by the symbol layer's own "name", not a separate attribute. */
typedef cgem_object_t cgem_ref_t;

cgem_ref_t *cgem_ref_new(const cgem_char_t *path, cgem_node_t *owner);
const cgem_char_t *cgem_ref_get_path(const cgem_ref_t *ref);

#endif
