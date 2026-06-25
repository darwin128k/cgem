#ifndef LH_INTERVAL_BOUNDS_FIELDS_H
#define LH_INTERVAL_BOUNDS_FIELDS_H

#include "lh/pair/fields.h"

/**
 * @brief Half-open interval with typed lower and upper bounds.
 *
 * @param type
 */
#define lh_interval_bounds_fields(type) lh_pair_fields(type, type)

#endif /* LH_INTERVAL_BOUNDS_FIELDS_H */
