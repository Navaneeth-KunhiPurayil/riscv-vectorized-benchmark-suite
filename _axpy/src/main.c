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

/*************************************************************************/

#ifndef USE_RISCV_VECTOR
    void axpy_serial(double a, double *dx, double *dy, int n);
#else
    void axpy_vector(double a, double *dx, double *dy, int n);
#endif

extern char end;

int main(int argc, char *argv[])
{
    long n;

    if (argc == 2)
    n = 1024*atol(argv[1]); // input argument: vector size in Ks
    else
        n = 1024;

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
    axpy_vector(a, dx, dy, n);
#endif

    printf ("done\n");
    test_result(dy, reference, n);

    return 0;
}
