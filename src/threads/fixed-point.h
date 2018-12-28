#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#include <debug.h>
#include <stdint.h>

typedef int32_t fp_t;
#define FP_SHIFT_AMOUNT 14
#define F0 (1 << FP_SHIFT_AMOUNT)
#define INT_TO_FP(N) ((fp_t)(N * F0))
#define FP_TO_INT_ROUND_ZERO(M) ((int32_t)(M / F0))
#define FP_TO_INT_ROUND_NEAR(M) (M >= 0 ? (int32_t)((M + F0 / 2) / F0) : (int32_t)((M - F0 / 2) / F0))
#define FP_ADD(M1, M2) (M1 + M2)
#define FP_INT_ADD(M, N) (M + N * F0)
#define FP_SUB(M1, M2) (M1 - M2)
#define FP_INT_SUB(M, N) (M - N * F0)
#define FP_MUL(M1, M2) ((fp_t)(((int64_t)M1) * M2 / F0))
#define FP_INT_MUL(M, N) ((fp_t)(M * N))
#define FP_DIV(M1, M2) ((fp_t)(((int64_t)M1) * F0 / M2))
#define FP_INT_DIV(M, N) ((fp_t)(M / N))

#endif

/* Ref: https://web.stanford.edu/class/cs140/projects/pintos/pintos_7.html#SEC131 */