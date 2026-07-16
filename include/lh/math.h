#ifndef LH_MATH_H
#define LH_MATH_H

#define lh_math_add(a, b) ((a) + (b))

#define lh_math_add_one(a) lh_math_add(a, 1)

#define lh_math_sub(a, b) ((a) - (b))

#define lh_math_sub_one(a) lh_math_sub(a, 1)

#define lh_math_mul(a, b) ((a) * (b))

#define lh_math_div(a, b) ((a) / (b))

#define lh_math_mod(a, b) ((a) % (b))

#define lh_math_neg(a) (-(a))

#define lh_math_eq(a, b) ((a) == (b))

#define lh_math_ne(a, b) ((a) != (b))

#define lh_math_lt(a, b) ((a) < (b))

#define lh_math_le(a, b) ((a) <= (b))

#define lh_math_gt(a, b) ((a) > (b))

#define lh_math_ge(a, b) ((a) >= (b))

#define lh_math_min(a, b) lh_math_lt(a, b) ? a : b

#define lh_math_max(a, b) lh_math_gt(a, b) ? a : b

#define lh_math_clamp(v, lo, hi) lh_math_min(lh_math_max(v, lo), hi)

#define lh_math_is_zero(v) lh_math_eq(a, 0)

#define lh_math_is_positive(v) lh_math_gt(v, 0)

#define lh_math_is_negative(v) lh_math_lt(v, 0)

#define lh_math_bit_and(a, b) ((a) & (b))

#define lh_math_bit_or(a, b) ((a) | (b))

#define lh_math_bit_xor(a, b) ((a) ^ (b))

#define lh_math_bit_not(v) (~(v))

#define lh_math_bit_shl(a, b) ((a) << (b))

#define lh_math_bit_shr(a, b) ((a) >> (b))

#define lh_math_bit_mask(n) lh_math_bit_shl(1u, n)

#endif /* LH_MATH_H */
