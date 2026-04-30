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

double *dx, *dy;
double reference;

int main(int hart_id)
{

    double a=1.53;

    if (hart_id == 0) { 
        printf("Running AXPY VL=%d with AraXL config L=%d C=%d cores=%d\n", n, NR_LANES, NR_CLUSTERS, NR_CORES);

        // /* Allocate the source and result vectors */
        dx     = (double*)baremetal_malloc(n*sizeof(double));
        dy     = (double*)baremetal_malloc(n*sizeof(double));

        init_vector(dx, n, 1.83);
        init_vector(dy, n, 2.22);

        reference = capture_ref_result(a, dx, dy, n);
    }

#if NR_CORES > 1 
    sync_barrier();
#endif

    double *dx_core = dx + (n / NR_CORES) * hart_id;
    double *dy_core = dy + (n / NR_CORES) * hart_id;

    if (hart_id == 0) 
        start_timer();

#ifndef USE_RISCV_VECTOR
    axpy_serial(a, dx_core, dy_core, n / NR_CORES);
#else
    axpy_vector(a, dx_core, dy_core, n / NR_CORES);
#endif
    
    if (hart_id == 0)
        stop_timer();

#if NR_CORES > 1 
    sync_barrier();
#endif

    if (hart_id == 0) {
        int64_t cycles = get_timer();

        float utilization = 100.0 * n    / (NR_LANES * NR_CLUSTERS * NR_CORES * cycles);
        printf ("[sw-cycles]: %ld util:%f%%\n", cycles, utilization);
    
        test_result(dy, reference, n);
    }

#if NR_CORES > 1 
    sync_barrier();
#endif

    return 0;
}
