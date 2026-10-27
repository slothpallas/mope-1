/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's maths: what the engine and the screen need, without libm (the device has none).
 *
 * Every function is a small static inline, in float, and none may get a NaN or an infinity. How each
 * one works, in short:
 *   exp2   2^x = 2^n * 2^r with n the nearest integer and |r| <= 1/2: n goes straight into the
 *          exponent bits, 2^r = e^(r ln 2) is its Taylor series to the 7th power (error < 2e-7)
 *   log2   x = 2^e * m, m folded into [0.707, 1.414); log2 m = (2 / ln 2) atanh(z), z = (m-1)/(m+1),
 *          |z| < 0.172, as the odd series z + z^3/3 + ... + z^9/9
 *   sine   a phase in turns, folded to a quarter wave around 0 (|t| <= 1/4), sin(2 pi t) as its odd
 *          Taylor series to the 13th power
 *   tanh   below 2, Lambert's continued fraction cut after seven terms (one divide, no exp); above,
 *          1 - 2 / (e^2x + 1)
 *   sqrt   the exponent halved for a first guess, then three Newton (Heron) steps
 * tests/host/mopemath_test.c holds each one to its error against the host's libm. */
#ifndef MOPEMATH_H
#define MOPEMATH_H
#include <stdint.h>

#define MM_PI 3.14159265358979f
#define MM_TWO_PI 6.28318530717959f
#define MM_LN2 0.693147180559945f
#define MM_LOG2E 1.44269504088896f

typedef union {
    float f;
    uint32_t u;
    int32_t i;
} mm_bits_t;

static inline float mm_fabsf(float x) { mm_bits_t b; b.f = x; b.u &= 0x7FFFFFFFu; return b.f; }
static inline float mm_minf(float a, float b) { return a < b ? a : b; }
static inline float mm_maxf(float a, float b) { return a > b ? a : b; }
static inline float mm_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
/* below 1e-20: zero (a denormal in a feedback loop is slow on some FPUs, and never heard) */
static inline float mm_flush(float x) { return mm_fabsf(x) < 1e-20f ? 0.0f : x; }

static inline float mm_floorf(float x)
{
    const float t = (float)(int32_t)x;                  /* toward zero ... */
    return t > x ? t - 1.0f : t;                        /* ... then down, for negatives */
}

static inline float mm_exp2f(float x)
{
    mm_bits_t p;
    int32_t n;
    float r, y;
    if (x < -126.0f)
        return 0.0f;
    if (x > 127.0f)
        x = 127.0f;
    n = (int32_t)(x < 0.0f ? x - 0.5f : x + 0.5f);       /* nearest whole power */
    r = (x - (float)n) * MM_LN2;                         /* |r| <= ln2 / 2 */
    y = 1.0f + r * (1.0f + r * (0.5f + r * (0.166666667f + r * (0.0416666667f + r * (0.00833333333f +
        r * (0.00138888889f + r * 0.000198412698f))))));
    if (n < -126) {                                      /* (the bottom of the range: in two steps) */
        p.u = (uint32_t)(n + 126 + 127) << 23;
        return y * p.f * 1.17549435e-38f;
    }
    p.u = (uint32_t)(n + 127) << 23;
    return y * p.f;
}
static inline float mm_expf(float x) { return mm_exp2f(x * MM_LOG2E); }

static inline float mm_log2f(float x)
{
    mm_bits_t b;
    int32_t e;
    float m, z, z2;
    if (!(x > 0.0f))
        return -128.0f;                                  /* (log of nothing: far below any level used) */
    b.f = x;
    e = (int32_t)((b.u >> 23) & 0xFFu) - 127;
    b.u = (b.u & 0x007FFFFFu) | 0x3F800000u;             /* the mantissa, in [1, 2) */
    m = b.f;
    if (m > 1.41421356f) {                               /* fold to [0.707, 1.414) */
        m *= 0.5f;
        e += 1;
    }
    z = (m - 1.0f) / (m + 1.0f);
    z2 = z * z;
    return (float)e + 2.88539008f * z * (1.0f + z2 * (0.333333333f + z2 * (0.2f + z2 * (0.142857143f + z2 * 0.111111111f))));
}
static inline float mm_logf(float x) { return mm_log2f(x) * MM_LN2; }
static inline float mm_powf(float x, float y) { return x > 0.0f ? mm_exp2f(y * mm_log2f(x)) : 0.0f; }

/* a sine of a phase in turns: one cycle is 0..1 */
static inline float mm_sin_turns(float ph)
{
    float t = ph - mm_floorf(ph + 0.5f), w, w2;          /* [-1/2, 1/2) */
    if (t > 0.25f)                                       /* the far quarters mirror the near ones */
        t = 0.5f - t;
    else if (t < -0.25f)
        t = -0.5f - t;
    w = t * MM_TWO_PI;                                   /* |w| <= pi/2 */
    w2 = w * w;
    return w * (1.0f + w2 * (-0.166666667f + w2 * (0.00833333333f + w2 * (-0.000198412698f + w2 * (2.75573192e-6f +
           w2 * (-2.50521084e-8f + w2 * 1.60590438e-10f))))));
}
static inline float mm_sinf(float x) { return mm_sin_turns(x * (1.0f / MM_TWO_PI)); }
static inline float mm_cosf(float x) { return mm_sin_turns(x * (1.0f / MM_TWO_PI) + 0.25f); }
static inline float mm_tanf(float x)
{
    const float c = mm_cosf(x);
    return mm_sinf(x) / (mm_fabsf(c) < 1e-9f ? 1e-9f : c);
}

static inline float mm_tanhf(float x)
{
    float a = mm_fabsf(x), x2, t;
    if (a < 2.0f) {                                      /* where most samples are: one divide, no exp */
        x2 = x * x;
        return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) /
               (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
    }
    if (a > 10.0f)
        return x > 0.0f ? 1.0f : -1.0f;
    t = 1.0f - 2.0f / (mm_exp2f(2.0f * MM_LOG2E * a) + 1.0f);   /* beyond: tanh a = 1 - 2 / (e^2a + 1) */
    return x > 0.0f ? t : -t;
}

static inline float mm_sqrtf(float x)
{
    mm_bits_t b;
    float y;
    if (!(x > 0.0f))
        return 0.0f;
    b.f = x;
    b.u = (b.u >> 1) + 0x1FC00000u;                      /* half the exponent: within a few % of the root */
    y = b.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return 0.5f * (y + x / y);
}
static inline float mm_rsqrtf(float x) { return x > 0.0f ? 1.0f / mm_sqrtf(x) : 0.0f; }

#endif
