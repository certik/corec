// Differential accuracy test and performance benchmark of corec's base/math.c
// against the host C library's <math.h> (-lm).
//
// Build with -DTEST_LIBM_SANITY first to verify the test assertions themselves
// against libm, then build normally to test and benchmark corec's fast_*
// implementations against libm.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef TEST_LIBM_SANITY
double fast_sin(double x);
double fast_cos(double x);
double fast_tan(double x);
double fast_exp(double x);
double fast_log(double x);
double fast_pow(double x, double y);
double base_floor(double x);
double base_ceil(double x);
double base_fmod(double x, double y);

float fast_sinf(float x);
float fast_cosf(float x);
float fast_tanf(float x);
float fast_expf(float x);
float fast_logf(float x);
float fast_powf(float x, float y);
float base_floorf(float x);
float base_ceilf(float x);
float base_fmodf(float x, float y);
#else
#define fast_sin   sin
#define fast_cos   cos
#define fast_tan   tan
#define fast_exp   exp
#define fast_log   log
#define fast_pow   pow
#define base_floor floor
#define base_ceil  ceil
#define base_fmod  fmod

#define fast_sinf   sinf
#define fast_cosf   cosf
#define fast_tanf   tanf
#define fast_expf   expf
#define fast_logf   logf
#define fast_powf   powf
#define base_floorf floorf
#define base_ceilf  ceilf
#define base_fmodf  fmodf
#endif

static int failures = 0;

static double rel_err_d(double got, double ref) {
    double diff = fabs(got - ref);
    double denom = fabs(ref);
    if (denom < 1e-12) return diff;
    return diff / denom;
}

static double rel_err_f(float got, float ref) {
    double diff = fabs((double)got - (double)ref);
    double denom = fabs((double)ref);
    if (denom < 1e-6) return diff;
    return diff / denom;
}

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

// Sanity-check mathematical identities (verifies the test suite on libm AND corec).
static void test_identities(void) {
    const double PI = 3.14159265358979323846;

    // sin^2 + cos^2 == 1, tan == sin/cos
    for (int i = -2000; i <= 2000; i++) {
        double x = (double)i * 0.0317;
        double s = fast_sin(x);
        double c = fast_cos(x);
        double err = fabs(s * s + c * c - 1.0);
        if (err > 1e-14) {
            printf("FAIL identity sin^2+cos^2 at x=%g: err=%e\n", x, err);
            failures++;
            break;
        }
        if (fabs(c) > 0.1) {
            double t = fast_tan(x);
            double terr = rel_err_d(t, s / c);
            if (terr > 1e-14) {
                printf("FAIL identity tan==sin/cos at x=%g: err=%e\n", x, terr);
                failures++;
                break;
            }
        }
    }

    // exp(log(x)) == x and log(exp(x)) == x
    for (int i = 1; i <= 2000; i++) {
        double x = (double)i * 0.137;
        double e1 = rel_err_d(fast_exp(fast_log(x)), x);
        if (e1 > 1e-13) {
            printf("FAIL identity exp(log(x)) at x=%g: err=%e\n", x, e1);
            failures++;
            break;
        }
    }
    for (int i = -500; i <= 500; i++) {
        double x = (double)i * 0.5;
        double e2 = rel_err_d(fast_log(fast_exp(x)), x);
        if (e2 > 1e-13) {
            printf("FAIL identity log(exp(x)) at x=%g: err=%e\n", x, e2);
            failures++;
            break;
        }
    }

    // Special values
    if (fabs(fast_sin(0.0)) > 1e-15 || fabs(fast_cos(0.0) - 1.0) > 1e-15 ||
        fabs(fast_sin(PI * 0.5) - 1.0) > 1e-14 || fabs(fast_cos(PI) + 1.0) > 1e-14 ||
        fabs(fast_tan(PI * 0.25) - 1.0) > 1e-14 ||
        fabs(fast_exp(0.0) - 1.0) > 1e-15 || fabs(fast_log(1.0)) > 1e-15 ||
        fabs(fast_pow(2.0, 10.0) - 1024.0) > 1e-12 ||
        fabs(fast_pow(3.0, 0.0) - 1.0) > 1e-15 ||
        fabs(fast_pow(-2.0, 3.0) + 8.0) > 1e-13) {
        printf("FAIL special values\n");
        failures++;
    }

    // floor / ceil / fmod
    if (base_floor(2.7) != 2.0 || base_floor(-2.7) != -3.0 || base_floor(2.0) != 2.0 ||
        base_ceil(2.1) != 3.0 || base_ceil(-2.1) != -2.0 || base_ceil(-2.0) != -2.0 ||
        fabs(base_fmod(5.5, 2.0) - 1.5) > 1e-15 || fabs(base_fmod(-5.5, 2.0) + 1.5) > 1e-15) {
        printf("FAIL floor/ceil/fmod\n");
        failures++;
    }
    if (base_floorf(2.7f) != 2.0f || base_floorf(-2.7f) != -3.0f ||
        base_ceilf(2.1f) != 3.0f || base_ceilf(-2.1f) != -2.0f ||
        fabsf(base_fmodf(5.5f, 2.0f) - 1.5f) > 1e-6f) {
        printf("FAIL floorf/ceilf/fmodf\n");
        failures++;
    }
}

static void check_1d(const char *name, double (*fn)(double), double (*ref_fn)(double),
                     double lo, double hi, int n, double tol, int avoid_poles) {
    double max_rel = 0.0;
    double worst_x = 0.0;
    for (int i = 0; i <= n; i++) {
        double x = lo + (hi - lo) * ((double)i / (double)n);
        if (avoid_poles && fabs(cos(x)) < 1e-3) continue;
        double got = fn(x);
        double ref = ref_fn(x);
        double err = rel_err_d(got, ref);
        if (err > max_rel) {
            max_rel = err;
            worst_x = x;
        }
    }
    int ok = (max_rel <= tol);
    printf("  %-12s [%-8g, %-8g] (%d pts): max_rel = %.3e (at x=%g)  %s\n",
           name, lo, hi, n, max_rel, worst_x, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static void check_1f(const char *name, float (*fn)(float), float (*ref_fn)(float),
                     float lo, float hi, int n, double tol, int avoid_poles) {
    double max_rel = 0.0;
    float worst_x = 0.0f;
    for (int i = 0; i <= n; i++) {
        float x = lo + (hi - lo) * ((float)i / (float)n);
        if (avoid_poles && fabsf(cosf(x)) < 1e-2f) continue;
        float got = fn(x);
        float ref = ref_fn(x);
        double err = rel_err_f(got, ref);
        if (err > max_rel) {
            max_rel = err;
            worst_x = x;
        }
    }
    int ok = (max_rel <= tol);
    printf("  %-12s [%-8g, %-8g] (%d pts): max_rel = %.3e (at x=%g)  %s\n",
           name, (double)lo, (double)hi, n, max_rel, (double)worst_x, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

static void check_pow(void) {
    double max_rel_d = 0.0, worst_x = 0.0, worst_y = 0.0;
    double xs[] = {0.001, 0.1, 0.5, 0.99, 1.0, 1.0001, 1.5, 2.0, 2.718281828, 10.0, 123.456, 1e6};
    double ys[] = {-20.0, -3.5, -1.0, -0.5, -0.01, 0.0, 0.01, 0.5, 1.0, 2.5, 10.0, 50.0};
    int nx = (int)(sizeof(xs) / sizeof(xs[0]));
    int ny = (int)(sizeof(ys) / sizeof(ys[0]));
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            double x = xs[i], y = ys[j];
            double ref = pow(x, y);
            if (!isfinite(ref) || ref == 0.0 || fabs(ref) < 1e-250 || fabs(ref) > 1e250) continue;
            double got = fast_pow(x, y);
            double err = rel_err_d(got, ref);
            if (err > max_rel_d) {
                max_rel_d = err;
                worst_x = x;
                worst_y = y;
            }
        }
    }
    int ok_d = (max_rel_d <= 1e-14);
    printf("  %-12s (grid %dx%d):            max_rel = %.3e (at %g^%g)  %s\n",
           "fast_pow", nx, ny, max_rel_d, worst_x, worst_y, ok_d ? "PASS" : "FAIL");
    if (!ok_d) failures++;

    double max_rel_f = 0.0;
    float wxf = 0.0f, wyf = 0.0f;
    for (int i = 0; i < nx; i++) {
        for (int j = 0; j < ny; j++) {
            float x = (float)xs[i], y = (float)ys[j];
            float ref = powf(x, y);
            if (!isfinite(ref) || ref == 0.0f || fabsf(ref) < 1e-30f || fabsf(ref) > 1e30f) continue;
            float got = fast_powf(x, y);
            double err = rel_err_f(got, ref);
            if (err > max_rel_f) {
                max_rel_f = err;
                wxf = x;
                wyf = y;
            }
        }
    }
    int ok_f = (max_rel_f <= 1e-6);
    printf("  %-12s (grid %dx%d):            max_rel = %.3e (at %g^%g)  %s\n",
           "fast_powf", nx, ny, max_rel_f, (double)wxf, (double)wyf, ok_f ? "PASS" : "FAIL");
    if (!ok_f) failures++;
}

#ifndef TEST_LIBM_SANITY
static volatile double sink_d = 0.0;
static volatile float sink_f = 0.0f;

static void bench_1d(const char *name, double (*fn)(double), double (*ref_fn)(double),
                     double lo, double hi) {
    const int N = 4096;
    const int REPS = 2000;
    double xs[4096];
    for (int i = 0; i < N; i++) xs[i] = lo + (hi - lo) * ((double)i / (double)N);

    double t0 = now_ns();
    double acc = 0.0;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc += fn(xs[i]);
    double t1 = now_ns();
    sink_d = acc;

    double t2 = now_ns();
    double acc2 = 0.0;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc2 += ref_fn(xs[i]);
    double t3 = now_ns();
    sink_d = acc2;

    double ns_fast = (t1 - t0) / ((double)N * REPS);
    double ns_libm = (t3 - t2) / ((double)N * REPS);
    printf("  %-12s corec = %6.2f ns/call, libm = %6.2f ns/call  (%.2fx)\n",
           name, ns_fast, ns_libm, ns_libm / ns_fast);
}

static void bench_1f(const char *name, float (*fn)(float), float (*ref_fn)(float),
                     float lo, float hi) {
    const int N = 4096;
    const int REPS = 2000;
    float xs[4096];
    for (int i = 0; i < N; i++) xs[i] = lo + (hi - lo) * ((float)i / (float)N);

    double t0 = now_ns();
    float acc = 0.0f;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc += fn(xs[i]);
    double t1 = now_ns();
    sink_f = acc;

    double t2 = now_ns();
    float acc2 = 0.0f;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc2 += ref_fn(xs[i]);
    double t3 = now_ns();
    sink_f = acc2;

    double ns_fast = (t1 - t0) / ((double)N * REPS);
    double ns_libm = (t3 - t2) / ((double)N * REPS);
    printf("  %-12s corec = %6.2f ns/call, libm = %6.2f ns/call  (%.2fx)\n",
           name, ns_fast, ns_libm, ns_libm / ns_fast);
}

static void bench_pow(void) {
    const int N = 4096;
    const int REPS = 1000;
    double xs[4096], ys[4096];
    float xsf[4096], ysf[4096];
    for (int i = 0; i < N; i++) {
        xs[i] = 0.25 + 10.0 * ((double)i / (double)N);
        ys[i] = -3.0 + 6.0 * ((double)((i * 73) % N) / (double)N);
        xsf[i] = (float)xs[i];
        ysf[i] = (float)ys[i];
    }

    double t0 = now_ns(), acc = 0.0;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc += fast_pow(xs[i], ys[i]);
    double t1 = now_ns();
    sink_d = acc;

    double t2 = now_ns(), acc2 = 0.0;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) acc2 += pow(xs[i], ys[i]);
    double t3 = now_ns();
    sink_d = acc2;
    printf("  %-12s corec = %6.2f ns/call, libm = %6.2f ns/call  (%.2fx)\n",
           "fast_pow", (t1 - t0) / ((double)N * REPS), (t3 - t2) / ((double)N * REPS),
           (t3 - t2) / (t1 - t0));

    double tf0 = now_ns();
    float accf = 0.0f;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) accf += fast_powf(xsf[i], ysf[i]);
    double tf1 = now_ns();
    sink_f = accf;

    double tf2 = now_ns();
    float accf2 = 0.0f;
    for (int r = 0; r < REPS; r++)
        for (int i = 0; i < N; i++) accf2 += powf(xsf[i], ysf[i]);
    double tf3 = now_ns();
    sink_f = accf2;
    printf("  %-12s corec = %6.2f ns/call, libm = %6.2f ns/call  (%.2fx)\n",
           "fast_powf", (tf1 - tf0) / ((double)N * REPS), (tf3 - tf2) / ((double)N * REPS),
           (tf3 - tf2) / (tf1 - tf0));
}
#endif

static void check_log_decades(void) {
    double max_rel_d = 0.0, worst_x = 0.0;
    for (int i = 0; i <= 100000; i++) {
        double p = -300.0 + 600.0 * ((double)i / 100000.0);
        double x = pow(10.0, p);
        double err = rel_err_d(fast_log(x), log(x));
        if (err > max_rel_d) { max_rel_d = err; worst_x = x; }
    }
    int ok_d = (max_rel_d <= 1e-14);
    printf("  %-12s [1e-300  , 1e+300  ] (log sweep): max_rel = %.3e (at x=%g)  %s\n",
           "fast_log", max_rel_d, worst_x, ok_d ? "PASS" : "FAIL");
    if (!ok_d) failures++;

    double max_rel_f = 0.0;
    float wxf = 0.0f;
    for (int i = 0; i <= 100000; i++) {
        double p = -35.0 + 70.0 * ((double)i / 100000.0);
        float x = (float)pow(10.0, p);
        double err = rel_err_f(fast_logf(x), logf(x));
        if (err > max_rel_f) { max_rel_f = err; wxf = x; }
    }
    int ok_f = (max_rel_f <= 1e-6);
    printf("  %-12s [1e-35   , 1e+35   ] (log sweep): max_rel = %.3e (at x=%g)  %s\n",
           "fast_logf", max_rel_f, (double)wxf, ok_f ? "PASS" : "FAIL");
    if (!ok_f) failures++;
}

int main(void) {
    printf("=== Math identity tests ===\n");
    test_identities();

    printf("=== Differential accuracy vs libm ===\n");
    check_1d("fast_sin",  fast_sin,  sin,  -1e4,   1e4,   200000, 1e-14, 0);
    check_1d("fast_cos",  fast_cos,  cos,  -1e4,   1e4,   200000, 1e-14, 0);
    check_1d("fast_tan",  fast_tan,  tan,  -1e4,   1e4,   200000, 1e-14, 1);
    check_1d("fast_exp",  fast_exp,  exp,  -700.0, 700.0, 200000, 1e-14, 0);
    check_1d("fast_log",  fast_log,  log,  1e-300, 1e300, 200000, 1e-14, 0);
    // Dense log sweep near 1 and small/moderate values
    check_1d("fast_log~1", fast_log, log,  0.001,  100.0, 200000, 1e-14, 0);

    check_1f("fast_sinf", fast_sinf, sinf, -1e3f,  1e3f,  200000, 1e-6,  0);
    check_1f("fast_cosf", fast_cosf, cosf, -1e3f,  1e3f,  200000, 1e-6,  0);
    check_1f("fast_tanf", fast_tanf, tanf, -1e3f,  1e3f,  200000, 1e-6,  1);
    check_1f("fast_expf", fast_expf, expf, -80.0f, 80.0f, 200000, 1e-6,  0);
    check_1f("fast_logf", fast_logf, logf, 1e-30f, 1e30f, 200000, 1e-6,  0);
    check_1f("fast_logf~1", fast_logf, logf, 0.001f, 100.0f, 200000, 1e-6, 0);
    check_log_decades();
    check_pow();

    if (failures > 0) {
        printf("FAILED: %d failure(s)\n", failures);
        return 1;
    }

#ifndef TEST_LIBM_SANITY
    printf("=== Performance benchmark (-O2/-O3) ===\n");
    bench_1d("fast_sin",  fast_sin,  sin,  -100.0, 100.0);
    bench_1d("fast_cos",  fast_cos,  cos,  -100.0, 100.0);
    bench_1d("fast_tan",  fast_tan,  tan,  -1.5,   1.5);
    bench_1d("fast_exp",  fast_exp,  exp,  -50.0,  50.0);
    bench_1d("fast_log",  fast_log,  log,  0.01,   1000.0);
    bench_1f("fast_sinf", fast_sinf, sinf, -100.0f, 100.0f);
    bench_1f("fast_cosf", fast_cosf, cosf, -100.0f, 100.0f);
    bench_1f("fast_tanf", fast_tanf, tanf, -1.5f,   1.5f);
    bench_1f("fast_expf", fast_expf, expf, -50.0f,  50.0f);
    bench_1f("fast_logf", fast_logf, logf, 0.01f,   1000.0f);
    bench_pow();
#endif

    printf("ALL MATH TESTS PASSED\n");
    return 0;
}
