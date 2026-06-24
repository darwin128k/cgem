#ifndef LH_BOOL_H
#define LH_BOOL_H

#include "lh/byte.h"

/**
 * @brief A boolean value represented by a byte.
 *
 * Use lh_bool_false and lh_bool_true as its named values.
 */
typedef lh_byte_t lh_bool_t;

static const lh_bool_t lh_bool_false = 0;
static const lh_bool_t lh_bool_true = 1;

#endif /* LH_BOOL_H */
