#include "cgem/core/call.h"

#include "cgem/core/symbol.h"

#define CGEM_CALL_TYPE "call"

cgem_call_t *cgem_call_new(const cgem_char_t *callee, cgem_node_t *owner)
{
    return cgem_object_new(callee, CGEM_CALL_TYPE, owner);
}

const cgem_char_t *cgem_call_get_callee(const cgem_call_t *call)
{
    return cgem_symbol_get_name((const cgem_symbol_t *) call);
}
