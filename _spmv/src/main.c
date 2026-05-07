/*
 * Neiel Israel Leyva Santes
 * neiel.leyva@bsc.es
 * Barcelona Supercomputing Center
 *
 * SpMV Vector Implementation.
 * Sparse Matrix-Vector Multiplication (SpMV) is a mathematical operation
 * in which a sparse matrix is multiplied by a dense vector.
 *
 * Inputs:
 *      *tiny:   football.mtx
 *               M. Girvan and M. E. J. Newman, The network of American football games
 *               between Division IA colleges during regular season Fall 2000.
 *      *small:  lhr07.mtx
 *               J. Mallya, Light hydrocarbon recovery. OK if ill conditioned, from a nonlinear solver.
 *      *medium: venkat25.mtx
 *               V. Venkatakrishnan, Unstructured 2d euler solver, time step = 25.
 *      *large:  poisson3Db.mtx
 *               Comsol, Inc., 3D Poisson problem.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include "common/riscv_util.h"
#define TOLERANCE 1e-6
#include "printf.h"
#include "runtime.h"

void spmv_intrinsics(const size_t nrows, double *a, uint64_t *ia, uint64_t *ja, double *x, double *y);
void spmv_serial(const size_t nrows, double *a, uint64_t *ia, uint64_t *ja, double *x, double *y);

extern size_t M, N, NZ;
extern uint64_t ia[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));
extern uint64_t ja[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));
extern double a[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));
extern double x[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));
extern double y[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));
extern double verif[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS)));

int main(int hart_id){

    if (hart_id == 0) {
        printf("Running SpMV with M=%ld, N=%ld, NNZ=%ld L=%d C=%d cores=%d\n", M, N, NZ, NR_LANES, NR_CLUSTERS, NR_CORES);
    }

#if NR_CORES > 1
    sync_barrier(); // all cores ready
#endif

    // Partition rows across cores; last core absorbs any remainder
    size_t M_per_core = M / NR_CORES;
    size_t M_start     = (size_t)hart_id * M_per_core;
    size_t M_end       = (hart_id == NR_CORES - 1) ? M : M_start + M_per_core;
    size_t M_rows      = M_end - M_start;

    if (hart_id == 0)
        start_timer();

#ifdef USE_RISCV_VECTOR
    spmv_intrinsics(M_rows, a, &ia[M_start], ja, x, &y[M_start]);
#else // !USE_RISCV_VECTOR
    spmv_serial(M_rows, a, &ia[M_start], ja, x, &y[M_start]);
#endif
    asm volatile ("fence");

#if NR_CORES > 1
    sync_barrier(); // all cores done writing y[]
#endif
    if (hart_id == 0)
        stop_timer();

    if (hart_id == 0) {
        for(size_t i=0; i < M ; i++){
            if (fabs(y[i] - verif[i]) > TOLERANCE) {
                printf("Verification fail \n");
                printf("idx: %ld %.17lf  -  %.17lf \n", i, y[i], verif[i]);
                return i+1;
            }
        }
        int64_t cycles = get_timer();
        int64_t total_ops = NZ; // 1 MAC op per non-zero element
        int64_t ops_per_cycle = NR_LANES * NR_CLUSTERS * NR_CORES; // all cores contribute
        int64_t theoretical_cycles = (total_ops + ops_per_cycle - 1) / ops_per_cycle; // Ceiling division
        float utilization = 100.0 * (float)theoretical_cycles / (float)cycles;
        printf("Verification pass\nSpMV execution took [sw-cycles]:%ld util:%f%%\n", cycles, utilization);
    }

#if NR_CORES > 1
    sync_barrier();
#endif

    return 0;
}
