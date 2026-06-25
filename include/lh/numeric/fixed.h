#ifndef LH_NUMERIC_FIXED_H
#define LH_NUMERIC_FIXED_H

#include "lh/numeric/types.h"
#include "lh/str/char.h"

/**
 * @brief A signed integer type with at least 8 bits.
 */
typedef lh_str_schar_t lh_s8_t;
/**
 * @brief A signed integer type with at least 16 bits.
 */
typedef lh_sshort_t lh_s16_t;
/**
 * @brief A signed integer type with at least 32 bits.
 */
typedef lh_sint_t lh_s32_t;
/**
 * @brief A signed integer type with at least 64 bits.
 */
typedef lh_sllong_t lh_s64_t;

/**
 * @brief An unsigned integer type with at least 8 bits.
 */
typedef lh_str_uchar_t lh_u8_t;
/**
 * @brief An unsigned integer type with at least 16 bits.
 */
typedef lh_ushort_t lh_u16_t;
/**
 * @brief An unsigned integer type with at least 32 bits.
 */
typedef lh_uint_t lh_u32_t;
/**
 * @brief An unsigned integer type with at least 64 bits.
 */
typedef lh_ullong_t lh_u64_t;

#endif /* LH_NUMERIC_FIXED_H */
