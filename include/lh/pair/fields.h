#ifndef LH_PAIR_FIELDS_H
#define LH_PAIR_FIELDS_H

/**
 * @brief A pair of typed values.
 *
 * @param first_type Type of the first value.
 * @param second_type Type of the second value.
 */
#define lh_pair_fields(first_type, second_type)                                \
    first_type first;                                                          \
    second_type second

#endif /* LH_PAIR_FIELDS_H */
