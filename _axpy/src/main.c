/*************************************************************************
* Axpy Kernel
* Author: Jesus Labarta
* Barcelona Supercomputing Center
*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "utils.h"

#include "printf.h"
#include "common/riscv_util.h"
#include "runtime.h"

/*************************************************************************/

#ifndef USE_RISCV_VECTOR
    void axpy_serial(double a, double *dx, double *dy, int n);
#else
    void axpy_vector(double a, double *dx, double *dy, int n);
#endif

extern char end;
extern int n;

int main()
{

    printf("Running AXPY VL=%d with AraXL config L=%d C=%d\n", n, NR_LANES, NR_CLUSTERS);

    // /* Allocate the source and result vectors */
    double *dx     = (double*)baremetal_malloc(n*sizeof(double));
    double *dy     = (double*)baremetal_malloc(n*sizeof(double));

    double a=1.53;
    init_vector(dx, n, 1.83);
    init_vector(dy, n, 2.22);

    double reference = capture_ref_result(a, dx, dy, n);

#ifndef USE_RISCV_VECTOR
    axpy_serial(a, dx, dy, n);
#else
    start_timer();
    axpy_vector(a, dx, dy, n);
    stop_timer();
#endif

    int64_t cycles = get_timer();
    printf ("[sw-cycles] %ld\n", cycles);

    test_result(dy, reference, n);

    return 0;
}
