/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/src/dsp/mopemath.h against the host's libm: each function's worst error over the range the
 * engine and the screen use it in, and the edges (nothing NaN, nothing infinite, nothing backwards). */
#include <math.h>
#include <stdio.h>
#include "mopemath.h"

static int fails;
static void check(const char *what, double worst, double limit)
{
    const int ok = worst <= limit && worst == worst;
    printf("  %s %-34s worst %.2e (limit %.0e)\n", ok ? "ok  " : "FAIL", what, worst, limit);
    fails += !ok;
}
#define SWEEP(lo, hi, n, body)                                       \
    do {                                                             \
        int i_;                                                      \
        for (i_ = 0; i_ <= (n); i_++) {                              \
            const float x = (float)((lo) + ((hi) - (lo)) * i_ / (n)); \
            body;                                                    \
        }                                                            \
    } while (0)
static double rel(double a, double b) { return fabs(a - b) / (fabs(b) > 1e-30 ? fabs(b) : 1e-30); }

int main(void)
{
    double w;

    printf("mopemath\n");
    w = 0; SWEEP(-30.0, 30.0, 200000, { double d = rel(mm_exp2f(x), exp2((double)x)); w = d > w ? d : w; });
    check("exp2, -30..30 (relative)", w, 3e-7);
    w = 0; SWEEP(-20.0, 20.0, 200000, { double d = rel(mm_expf(x), exp((double)x)); w = d > w ? d : w; });
    check("exp, -20..20 (relative)", w, 3e-6);
    w = 0; SWEEP(1e-6, 1000.0, 400000, { double d = fabs(mm_log2f(x) - log2((double)x)); w = d > w ? d : w; });
    check("log2, 1e-6..1000", w, 3e-6);
    w = 0; SWEEP(0.01, 4.0, 100000, { double d = rel(mm_powf(x, 2.7f), pow((double)x, 2.7)); w = d > w ? d : w; });
    check("pow(x, 2.7), 0.01..4 (relative)", w, 2e-5);
    w = 0; SWEEP(-20.0, 20.0, 400000, { double d = fabs(mm_sin_turns(x) - sin(2.0 * M_PI * (double)x)); w = d > w ? d : w; });
    check("sin of turns, -20..20", w, 3e-6);
    w = 0; SWEEP(-40.0, 40.0, 400000, { double d = fabs(mm_sinf(x) - sin((double)x)); w = d > w ? d : w; });
    check("sin, -40..40", w, 1e-5);
    w = 0; SWEEP(-40.0, 40.0, 400000, { double d = fabs(mm_cosf(x) - cos((double)x)); w = d > w ? d : w; });
    check("cos, -40..40", w, 1e-5);
    w = 0; SWEEP(0.0001, 1.5, 100000, { double d = rel(mm_tanf(x), tan((double)x)); w = d > w ? d : w; });
    check("tan, 0..1.5 (relative)", w, 2e-5);
    w = 0; SWEEP(-12.0, 12.0, 400000, { double d = fabs(mm_tanhf(x) - tanh((double)x)); w = d > w ? d : w; });
    check("tanh, -12..12", w, 2e-5);
    w = 0; SWEEP(1e-8, 1e6, 400000, { double d = rel(mm_sqrtf(x), sqrt((double)x)); w = d > w ? d : w; });
    check("sqrt, 1e-8..1e6 (relative)", w, 2e-7);
    w = 0; SWEEP(-50.0, 50.0, 100000, { double d = fabs(mm_floorf(x) - floor((double)x)); w = d > w ? d : w; });
    check("floor, -50..50", w, 0);
    {                                                    /* the edges */
        int bad = 0, back = 0;
        float prev = -2.0f;
        bad += mm_exp2f(-500.0f) != 0.0f;
        bad += !(mm_exp2f(500.0f) < 3.4e38f);
        bad += mm_log2f(0.0f) > -100.0f;
        bad += mm_sqrtf(-1.0f) != 0.0f;
        bad += mm_powf(0.0f, 2.0f) != 0.0f;
        SWEEP(-6.0, 6.0, 120000, { const float t = mm_tanhf(x); back += t < prev; prev = t; bad += fabsf(t) > 1.0f; });
        check("edges: no NaN, no inf, in range", (double)bad, 0);
        check("tanh never goes backwards", (double)back, 0);
    }
    printf(fails ? "MOPEMATH TESTS FAILED (%d)\n" : "mopemath: all passed\n", fails);
    return fails != 0;
}
