/*
 * =======================================================================================
 *
 *      Author:   Jan Eitzinger (je), jan.treibig@gmail.com
 *      Copyright (c) 2019 RRZE, University Erlangen-Nuremberg
 *
 *      Permission is hereby granted, free of charge, to any person obtaining a copy
 *      of this software and associated documentation files (the "Software"), to deal
 *      in the Software without restriction, including without limitation the rights
 *      to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *      copies of the Software, and to permit persons to whom the Software is
 *      furnished to do so, subject to the following conditions:
 *
 *      The above copyright notice and this permission notice shall be included in all
 *      copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *      IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *      FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *      AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *      LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *      OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *      SOFTWARE.#include <stdlib.h>
 *
 * =======================================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <float.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <allocate.h>

#define ARRAY_ALIGNMENT	64
#define SIZE	20000000ull
#define NTIMES	10

# define HLINE "-------------------------------------------------------------\n"

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif
#ifndef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))
#endif
#ifndef ABS
#define ABS(a) ((a) >= 0 ? (a) : -(a))
#endif

typedef enum benchmark {
    INIT = 0,
    SUM,
    COPY,
    UPDATE,
    TRIAD,
    DAXPY,
    STRIAD,
    SDAXPY,
    NUMBENCH
} benchmark;

extern double init(double*, double, int);
extern double sum(double*, int);
extern double copy(double*, double*, int);
extern double update(double*, double, int);
extern double triad(double*, double*, double*, double, int);
extern double daxpy(double*, double*, double, int);
extern double striad(double*, double*, double*, double*, int);
extern double sdaxpy(double*, double*, double*, int);

void check(double*, double*, double*, double*, int);

int main (int argc, char** argv)
{
    size_t bytesPerWord = sizeof(double);
    size_t N = SIZE;
    double *a, *b, *c, *d;
    double scalar, tmp;

    double	avgtime[NUMBENCH],
            maxtime[NUMBENCH],
            mintime[NUMBENCH];

    double times[NUMBENCH][NTIMES];

    double	bytes[NUMBENCH] = {
        1 * sizeof(double) * N, /* init */
        1 * sizeof(double) * N, /* sum */
        2 * sizeof(double) * N, /* copy */
        2 * sizeof(double) * N, /* update */
        3 * sizeof(double) * N, /* triad */
        3 * sizeof(double) * N, /* daxpy */
        4 * sizeof(double) * N, /* striad */
        4 * sizeof(double) * N  /* sdaxpy */
    };

    char	*label[NUMBENCH] = {
        "Init:       ",
        "Sum:        ",
        "Copy:       ",
        "Update:     ",
        "Triad:      ",
        "Daxpy:      ",
        "STriad:     ",
        "SDaxpy:     "};

    a = (double*) allocate( ARRAY_ALIGNMENT, N * bytesPerWord );
    b = (double*) allocate( ARRAY_ALIGNMENT, N * bytesPerWord );
    c = (double*) allocate( ARRAY_ALIGNMENT, N * bytesPerWord );
    d = (double*) allocate( ARRAY_ALIGNMENT, N * bytesPerWord );

    for (int i=0; i<NUMBENCH; i++) {
        avgtime[i] = 0;
        maxtime[i] = 0;
        mintime[i] = FLT_MAX;
    }

#ifdef _OPENMP
    printf(HLINE);
#pragma omp parallel
    {
        int k = omp_get_num_threads();

#pragma omp single
        printf ("OpenMP enabled, running with %d threads\n", k);
    }
#endif

#pragma omp parallel for
    for (int i=0; i<N; i++) {
        a[i] = 2.0;
        b[i] = 2.0;
        c[i] = 0.5;
        d[i] = 1.0;
    }

    scalar = 3.0;

    for ( int k=0; k < NTIMES; k++) {
        times[INIT][k]   = init(b, scalar, N);
        tmp = a[10];
        times[SUM][k]    = sum(a, N);
        a[10] = tmp;
        times[COPY][k]   = copy(c, a, N);
        times[UPDATE][k] = update(a, scalar, N);
        times[TRIAD][k]  = triad(a, b, c, scalar, N);
        times[DAXPY][k]  = daxpy(a, b, scalar, N);
        times[STRIAD][k] = striad(a, b, c, d, N);
        times[SDAXPY][k] = sdaxpy(a, b, c, N);
    }

    for (int j=0; j<NUMBENCH; j++) {
        for (int k=1; k<NTIMES; k++) {
            avgtime[j] = avgtime[j] + times[j][k];
            mintime[j] = MIN(mintime[j], times[j][k]);
            maxtime[j] = MAX(maxtime[j], times[j][k]);
        }
    }

    printf(HLINE);
    printf("Function      Rate (MB/s)   Avg time     Min time     Max time\n");
    for (int j=0; j<NUMBENCH; j++) {
        avgtime[j] = avgtime[j]/(double)(NTIMES-1);

        printf("%s%11.4f  %11.4f  %11.4f  %11.4f\n", label[j],
                1.0E-06 * bytes[j]/mintime[j],
                avgtime[j],
                mintime[j],
                maxtime[j]);
    }
    printf(HLINE);

    check(a, b, c, d, N);

    return EXIT_SUCCESS;
}

void check(
        double * a,
        double * b,
        double * c,
        double * d,
        int N
        )
{
    double aj, bj, cj, dj, scalar;
    double asum, bsum, csum, dsum;
    double epsilon;

    /* reproduce initialization */
    aj = 2.0;
    bj = 2.0;
    cj = 0.5;
    dj = 1.0;

    /* now execute timing loop */
    scalar = 3.0;

    for (int k=0; k<NTIMES; k++) {
        bj = scalar;
        cj = aj;
        aj = aj * scalar;
        aj = bj + scalar * cj;
        aj = aj + scalar * bj;
        aj = bj + cj * dj;
        aj = aj + bj * cj;
    }

    aj = aj * (double) (N);
    bj = bj * (double) (N);
    cj = cj * (double) (N);
    dj = dj * (double) (N);

    asum = 0.0; bsum = 0.0; csum = 0.0; dsum = 0.0;

    for (int i=0; i<N; i++) {
        asum += a[i];
        bsum += b[i];
        csum += c[i];
        dsum += d[i];
    }

#ifdef VERBOSE
    printf ("Results Comparison: \n");
    printf ("        Expected  : %f %f %f \n",aj,bj,cj);
    printf ("        Observed  : %f %f %f \n",asum,bsum,csum);
#endif

    epsilon = 1.e-8;

    if (ABS(aj-asum)/asum > epsilon) {
        printf ("Failed Validation on array a[]\n");
        printf ("        Expected  : %f \n",aj);
        printf ("        Observed  : %f \n",asum);
    }
    else if (ABS(bj-bsum)/bsum > epsilon) {
        printf ("Failed Validation on array b[]\n");
        printf ("        Expected  : %f \n",bj);
        printf ("        Observed  : %f \n",bsum);
    }
    else if (ABS(cj-csum)/csum > epsilon) {
        printf ("Failed Validation on array c[]\n");
        printf ("        Expected  : %f \n",cj);
        printf ("        Observed  : %f \n",csum);
    }
    else if (ABS(dj-dsum)/dsum > epsilon) {
        printf ("Failed Validation on array d[]\n");
        printf ("        Expected  : %f \n",dj);
        printf ("        Observed  : %f \n",dsum);
    }
    else {
        printf ("Solution Validates\n");
    }
}
