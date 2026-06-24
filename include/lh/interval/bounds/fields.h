#ifndef LH_INTERVAL_BOUNDS_FIELDS_H
#define LH_INTERVAL_BOUNDS_FIELDS_H

/**
 * @brief Half-open interval with typed lower and upper bounds.
 *
 * @param lower_type Lower bound (inclusive).
 * @param upper_type Upper bound (exclusive).
 */
#define lh_interval_bounds_fields(lower_type, upper_type)                      \
    const lower_type lower;                                                    \
    const upper_type upper

#endif /* LH_INTERVAL_BOUNDS_FIELDS_H */
