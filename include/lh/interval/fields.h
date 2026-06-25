#ifndef LH_INTERVAL_FIELDS_H
#define LH_INTERVAL_FIELDS_H

#include "lh/interval/flags.h"

/**
 * @param bounds_type
 */
#define lh_interval_fields(bounds_type)                                        \
    const bounds_type bounds;                                                  \
    const lh_interval_flags_t flags

#endif /* LH_INTERVAL_FIELDS_H */
