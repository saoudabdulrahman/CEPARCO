/*
 * Vector triad: A[i] = B[i] + C[i] * D[i], all single-precision floats.
 *
 * Four versions of the same loop:
 *   1. plain C                       (c_kernel)
 *   2. x86-64 scalar asm             (asm_x86_64)     -> x86-64.asm
 *   3. AVX with 128-bit XMM regs     (asm_simd_xmm)   -> simdxmm.asm  (4 floats per pass)
 *   4. AVX2 with 256-bit YMM regs    (asm_simd_ymm)   -> simdymm.asm  (8 floats per pass)
 *
 * Input data:
 *   B[i] = sin(i * 0.001)
 *   C[i] = cos(i * 0.002)
 *   D[i] = sin(i * 0.0005) + 1.5     (so D stays between 0.5 and 2.5)
 *
 * Timing: only the kernel call is timed, using QueryPerformanceCounter. Each
 * kernel gets one untimed warm-up run, then NUM_RUNS timed runs. We report the
 * average and the best (minimum) time.
 *
 * Correctness:
 *   - The C kernel's output is the reference. Each assembly kernel is compared
 *     against it element by element. If there's room in RAM for a saved copy of
 *     that output, we compare against the copy. If not (e.g. at 2^30), we
 *     recompute B[i] + C[i]*D[i] on the fly instead.
 *   - The C kernel is checked against a double-precision version of the same
 *     formula, so it isn't just checking itself.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <windows.h>

#define NUM_RUNS   30                /* timed runs per kernel */
#define PAD        16                /* extra slots after A, to catch writes past the end */
#define SENTINEL   (-12345.0f)       /* marker written into A before each run */

 /* All four kernels share this signature. */
typedef void (*kernel_fn)(unsigned long long n, float* A,
    const float* B, const float* C, const float* D);

/* Assembly kernels. Windows x64 calling convention: n in rcx, A in rdx, B in r8,
 * C in r9, and D is the 5th argument, so it's on the stack at [rsp+40]. */
extern void asm_x86_64(unsigned long long n, float* A, const float* B, const float* C, const float* D);
extern void asm_simd_xmm(unsigned long long n, float* A, const float* B, const float* C, const float* D);
extern void asm_simd_ymm(unsigned long long n, float* A, const float* B, const float* C, const float* D);

/* Reference C version. __restrict tells the compiler the arrays don't overlap.
 * noinline keeps it as a separate function so the timing reflects this exact code. */
__declspec(noinline)
void c_kernel(unsigned long long n, float* __restrict A,
    const float* __restrict B, const float* __restrict C, const float* __restrict D)
{
    for (unsigned long long i = 0; i < n; i++)
        A[i] = B[i] + C[i] * D[i];
}

/* A kernel's display name and its function pointer, kept together for the tables. */
typedef struct {
    const char* name;
    kernel_fn   fn;
} Kernel;

/* Current time in milliseconds from the high-resolution counter. */
static double now_ms(void)
{
    static LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER t;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/* Compares a against a stored reference. Returns how many elements are off by
 * more than a relative 1e-5, and the largest difference in *max_diff. The check
 * is written as !(d <= tol) so that NaN counts as a mismatch too. */
static unsigned long long compare(const float* a, const float* ref,
    unsigned long long n, float* max_diff)
{
    unsigned long long bad = 0;
    float md = 0.0f;
    for (unsigned long long i = 0; i < n; i++) {
        float d = fabsf(a[i] - ref[i]);
        float tol = 1e-5f * fmaxf(1.0f, fabsf(ref[i]));
        if (d > md) md = d;
        if (!(d <= tol)) bad++;       /* NaN fails this check, which is what we want */
    }
    *max_diff = md;
    return bad;
}

/* Same check as compare(), but the expected value is recomputed from B, C and D
 * with the same formula the C kernel uses. This avoids needing a stored copy of
 * the C output, which matters at the largest sizes. */
static unsigned long long compare_inputs(const float* a, const float* B, const float* C,
    const float* D, unsigned long long n, float* max_diff)
{
    unsigned long long bad = 0;
    float md = 0.0f;
    for (unsigned long long i = 0; i < n; i++) {
        float ref = B[i] + C[i] * D[i];
        float d = fabsf(a[i] - ref);
        float tol = 1e-5f * fmaxf(1.0f, fabsf(ref));
        if (d > md) md = d;
        if (!(d <= tol)) bad++;
    }
    *max_diff = md;
    return bad;
}

/* Sanity check for the C kernel. The expected value is computed in double
 * precision (inputs widened first), so it doesn't go through the same float
 * arithmetic the kernel uses. Same 1e-5 relative tolerance as the others. */
static unsigned long long compare_double(const float* a, const float* B, const float* C,
    const float* D, unsigned long long n, float* max_diff)
{
    unsigned long long bad = 0;
    double md = 0.0;
    for (unsigned long long i = 0; i < n; i++) {
        double ref = (double)B[i] + (double)C[i] * (double)D[i];
        double d = fabs((double)a[i] - ref);
        double tol = 1e-5 * fmax(1.0, fabs(ref));
        if (d > md) md = d;
        if (!(d <= tol)) bad++;
    }
    *max_diff = (float)md;
    return bad;
}

/* Checks whether `vecs` float arrays of length n fit in free physical RAM, with
 * some headroom to spare. We check physical RAM on purpose: malloc will happily
 * succeed against the pagefile, and then the run just thrashes the disk. */
static int ram_ok(unsigned long long n, int vecs, int verbose)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    double need = (double)vecs * (double)n * sizeof(float);
    double avail = (double)ms.ullAvailPhys;
    if (verbose)
        printf("  need %.2f GiB, available physical RAM %.2f GiB\n",
            need / 1073741824.0, avail / 1073741824.0);
    return need <= 0.75 * avail;
}

/* A run needs four arrays (A, B, C, D) at minimum. */
static int fits_in_ram(unsigned long long n)
{
    return ram_ok(n, 4, 1);
}

/* Prints the first and last five values of A, so we can eyeball the output. */
static void print_edges(const float* A, unsigned long long n)
{
    unsigned long long k = (n < 5) ? n : 5;
    printf("    first %llu: ", k);
    for (unsigned long long i = 0; i < k; i++) printf("%.6f ", A[i]);
    printf("\n    last  %llu: ", k);
    for (unsigned long long i = n - k; i < n; i++) printf("%.6f ", A[i]);
    printf("\n");
}

/* Fills the inputs with the values described at the top of the file. */
static void init_vectors(unsigned long long n, float* B, float* C, float* D)
{
    for (unsigned long long i = 0; i < n; i++) {
        B[i] = (float)sin((double)i * 0.001);
        C[i] = (float)cos((double)i * 0.002);
        D[i] = (float)(sin((double)i * 0.0005) + 1.5);
    }
}

/* Times only the kernel call. Returns the average in ms, and the best run in *min_ms. */
static double time_kernel(kernel_fn fn, unsigned long long n, float* A,
    const float* B, const float* C, const float* D, double* min_ms)
{
    double sum = 0.0, mn = 1e300;
    fn(n, A, B, C, D);                         /* untimed warm-up: faults in A's pages and warms caches/TLB */
    for (int r = 0; r < NUM_RUNS; r++) {
        double t0 = now_ms();
        fn(n, A, B, C, D);
        double t1 = now_ms();
        double dt = t1 - t0;
        sum += dt;
        if (dt < mn) mn = dt;
    }
    *min_ms = mn;
    return sum / NUM_RUNS;
}

/* Runs every kernel at one vector size. Returns 0 if the arrays couldn't be allocated. */
static int run_size(unsigned long long n, Kernel* ks, int nk)
{
    size_t bytes = (size_t)n * sizeof(float);
    float* B = (float*)_aligned_malloc(bytes, 32);
    float* C = (float*)_aligned_malloc(bytes, 32);
    float* D = (float*)_aligned_malloc(bytes, 32);
    float* A = (float*)_aligned_malloc(bytes, 32);

    if (!B || !C || !D || !A) {
        _aligned_free(B); _aligned_free(C); _aligned_free(D); _aligned_free(A);
        return 0;
    }

    /* Optional fifth array: a saved copy of the C output, used as the reference.
     * It's only allocated if there's room. Otherwise the kernels are checked
     * against the formula directly (see compare_inputs). */
    float* Aref = ram_ok(n, 5, 0) ? (float*)_aligned_malloc(bytes, 32) : NULL;

    printf("\n==================================================================\n");
    printf("n = %llu  (%.1f MiB per vector, %.1f MiB total)\n", n,
        bytes / 1048576.0, 4.0 * bytes / 1048576.0);
    printf("==================================================================\n");
    printf("Initializing vectors...\n");
    init_vectors(n, B, C, D);
    printf("C-output answer key array: %s\n",
        Aref ? "stored (kernels are compared against the actual C output)"
        : "not stored (not enough RAM; expected value recomputed with the C formula)");

    double avg[8], mn[8];
    for (int k = 0; k < nk; k++) {
        avg[k] = time_kernel(ks[k].fn, n, A, B, C, D, &mn[k]);

        printf("\n[%s]  avg = %.4f ms, min = %.4f ms  (%d runs)\n",
            ks[k].name, avg[k], mn[k], NUM_RUNS);
        print_edges(A, n);
        if (k == 0) {
            /* C is the reference. Save its output for the other comparisons, and
             * check it against the double-precision result as a sanity check. */
            if (Aref) memcpy(Aref, A, bytes);
            float md;
            unsigned long long bad = compare_double(A, B, C, D, n, &md);
            printf("    correctness: answer key; sanity check vs double-precision B+C*D: %s"
                "  (mismatches = %llu, max |diff| = %g)\n",
                bad == 0 ? "PASS" : "FAIL", bad, (double)md);
        }
        else {
            /* A holds this kernel's output from the timing runs. */
            float md;
            unsigned long long bad;
            if (Aref) {
                bad = compare(A, Aref, n, &md);
                printf("    correctness vs C output: %s  (mismatches = %llu, max |diff| = %g)\n",
                    bad == 0 ? "PASS" : "FAIL", bad, (double)md);
            }
            else {
                bad = compare_inputs(A, B, C, D, n, &md);
                printf("    correctness vs C formula (recomputed): %s  (mismatches = %llu, max |diff| = %g)\n",
                    bad == 0 ? "PASS" : "FAIL", bad, (double)md);
            }
        }
    }

    printf("\n  Summary (n = %llu)\n", n);
    printf("  %-18s %12s %12s %10s\n", "Kernel", "Avg (ms)", "Min (ms)", "Speedup");
    for (int k = 0; k < nk; k++)
        printf("  %-18s %12.4f %12.4f %9.2fx\n", ks[k].name, avg[k], mn[k], avg[0] / avg[k]);

    _aligned_free(B); _aligned_free(C); _aligned_free(D); _aligned_free(A);
    _aligned_free(Aref);          /* NULL-safe */
    return 1;
}

/* Boundary tests. We use sizes that aren't multiples of 4 or 8, so the leftover
 * loops actually run. Each kernel writes into A, which starts filled with the
 * sentinel, and the PAD slots past n must still hold the sentinel afterward. */
static void boundary_tests(Kernel* ks, int nk)
{
    static const unsigned long long sizes[] =
    { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 1003, (1ULL << 20) + 3 };
    const int ns = (int)(sizeof(sizes) / sizeof(sizes[0]));

    printf("\n==================================================================\n");
    printf("BOUNDARY TESTS (leftover elements + overrun guard)\n");
    printf("==================================================================\n");

    for (int s = 0; s < ns; s++) {
        unsigned long long n = sizes[s];
        size_t bytes = (size_t)(n + PAD) * sizeof(float);
        float* B = (float*)_aligned_malloc(bytes, 32);
        float* C = (float*)_aligned_malloc(bytes, 32);
        float* D = (float*)_aligned_malloc(bytes, 32);
        float* Aref = (float*)_aligned_malloc(bytes, 32);
        float* A = (float*)_aligned_malloc(bytes, 32);
        if (!B || !C || !D || !Aref || !A) { printf("alloc failed\n"); return; }

        init_vectors(n, B, C, D);
        c_kernel(n, Aref, B, C, D);

        printf("\nn = %llu  (n mod 4 = %llu, n mod 8 = %llu)\n", n, n % 4, n % 8);
        for (int k = 1; k < nk; k++) {
            for (unsigned long long i = 0; i < n + PAD; i++) A[i] = SENTINEL;
            ks[k].fn(n, A, B, C, D);

            float md;
            unsigned long long bad = compare(A, Aref, n, &md);
            int guard_ok = 1;
            for (int g = 0; g < PAD; g++)
                if (A[n + g] != SENTINEL) guard_ok = 0;

            printf("  %-18s %s  (mismatches = %llu, overrun guard %s)\n",
                ks[k].name, (bad == 0 && guard_ok) ? "PASS" : "FAIL",
                bad, guard_ok ? "intact" : "CORRUPTED");
            if (n == 1003 || n == 17) print_edges(A, n);
        }
        _aligned_free(B); _aligned_free(C); _aligned_free(D);
        _aligned_free(Aref); _aligned_free(A);
    }
}

int main(void)
{
    /* YMM needs AVX2. If the CPU or OS doesn't report it, quit before running anything. */
    if (!IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE)) {
        printf("This CPU/OS does not report AVX2 support. YMM kernel cannot run.\n");
        return 1;
    }

#ifdef _DEBUG
    printf("Build configuration: DEBUG\n");
#else
    printf("Build configuration: RELEASE\n");
#endif
    printf("Vector triad: A[i] = B[i] + C[i] * D[i]  (float)\n");
    printf("Init: B[i]=sin(i*0.001), C[i]=cos(i*0.002), D[i]=sin(i*0.0005)+1.5\n");

    Kernel ks[] = {
        { "C",            c_kernel },
        { "x86-64",       asm_x86_64 },
        { "SIMD XMM",     asm_simd_xmm },
        { "SIMD YMM",     asm_simd_ymm },
    };
    const int nk = (int)(sizeof(ks) / sizeof(ks[0]));

    /* Required sizes are 2^20, 2^26 and 2^30. If a size doesn't fit in RAM, step
     * down one power of two at a time (as the assignment allows), but never below 2^28. */
    static const int exps[] = { 20, 26, 30 };
    for (int e = 0; e < 3; e++) {
        int exp = exps[e];
        for (;;) {
            unsigned long long n = 1ULL << exp;
            printf("\nChecking memory for n = 2^%d:\n", exp);
            if (fits_in_ram(n) && run_size(n, ks, nk)) break;
            printf("  Not enough free physical RAM for 4 vectors of 2^%d floats (%.1f GiB each).\n",
                exp, (double)(n * 4) / 1073741824.0);
            if (exp > 28) { exp--; printf("  Reducing to 2^%d...\n", exp); }
            else { printf("  Skipping.\n"); break; }
        }
    }

    boundary_tests(ks, nk);

    printf("\nDone.\n");
    return 0;
}
