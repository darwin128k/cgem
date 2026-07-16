#ifndef LH_MATH_H
#define LH_MATH_H

#define lh_math_add(a, b) (a + b)

#define lh_math_add_one(a) (lh_math_add(a, 1))

#define lh_math_sub(a, b) (a - b)

#define lh_math_sub_one(a) (lh_math_sub(a, 1))

#define lh_math_mul(a, b) (a * b)

#define lh_math_div(a, b) (a / b)

#define lh_math_mod(a, b) (a % b)

#define lh_math_neg(a) (-a)

#define lh_math_eq(a, b) (a == b)

#define lh_math_ne(a, b) (a != b)

#define lh_math_lt(a, b) (a < b)

#define lh_math_le(a, b) (a <= b)

#define lh_math_gt(a, b) (a > b)

#define lh_math_ge(a, b) (a >= b)

#define lh_math_min(a, b) (((lh_math_lt(a, b)) ? (a) : (b)))

#define lh_math_max(a, b) (((lh_math_gt(a, b)) ? (a) : (b)))

#define lh_math_clamp(v, hi, lo) (lh_math_min(lh_math_max(v, lo), hi))

#endif /* LH_MATH_H */
