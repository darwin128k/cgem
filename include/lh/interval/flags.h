#ifndef LH_INTERVAL_FLAGS_H
#define LH_INTERVAL_FLAGS_H

#include "lh/byte.h"

typedef lh_byte_t lh_interval_flags_t;

static const lh_interval_flags_t lh_interval_flags_closed = 0;
static const lh_interval_flags_t lh_interval_flags_ropen = 1 << 0;
static const lh_interval_flags_t lh_interval_flags_lopen = 1 << 1;
static const lh_interval_flags_t lh_interval_flags_open =
    lh_interval_flags_lopen | lh_interval_flags_ropen;

#endif /* LH_INTERVAL_FLAGS_H */
