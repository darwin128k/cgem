#ifndef CGEM_CALL_H
#define CGEM_CALL_H

#include "cgem/core/object.h"
#include "cgem/core/primitive.h"

/* A call expression: invokes `callee` (a dotted path, e.g. "self.add" or
 * "add") with ordered argument expressions as children. This is how every
 * DSL operator (+, -, ., *, ...) is represented at the core level -- core
 * has no operators of its own, only calls to magic methods. Args are added
 * and read via the generic cgem_node_add/_get_count/_get, same as any
 * other homogeneous child list in this codebase (package/scope/module). */
typedef cgem_object_t cgem_call_t;

cgem_call_t *cgem_call_new(const cgem_char_t *callee, cgem_node_t *owner);
const cgem_char_t *cgem_call_get_callee(const cgem_call_t *call);

#endif
