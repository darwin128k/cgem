#ifndef LH_ERROR_H
#define LH_ERROR_H

#include "lh/error/code.h"
#include "lh/error/desc.h"
#include "lh/error/fields.h"

typedef struct lh_error
{
    lh_error_fields(lh_error_code_t, lh_error_desc);
} lh_error_t;

#endif /* LH_ERROR_H */
