#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>

using namespace std;

/*************************************************************************
* RISC-V Vectorized Version
* Author: Cristóbal Ramírez Lazo
* email: cristobal.ramirez@bsc.es
* Barcelona Supercomputing Center (2020)
*************************************************************************/

#include "common/riscv_util.h"
#include "printf.h"
#include "runtime.h"

#ifdef USE_RISCV_VECTOR
#include <riscv_vector.h>
#include "common/vector_defines.h"
#endif

/************************************************************************/

//Enable RESULT_PRINT in order to see the result vector, for instruction count it should be disable
// #define RESULT_PRINT
//Enable INPUT_PRINT in order to see the input matrix, for instruction count it should be disable
//#define INPUT_PRINT

#define MAXNAMESIZE 1024 // max filename length
#define M_SEED 9
#define MAX_WEIGHT 10
#define NUM_RUNS 1

extern uint32_t rows, cols;
extern int wall[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS), section(".l2")));
extern int result[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS), section(".l2")));
extern int reference[] __attribute__((aligned(4 * NR_LANES * NR_CLUSTERS), section(".l2")));

void run();
void run_vector(int hart_id);
void output_print(int *dst, int cols);

bool compare( int cols, int* result, int* reference);

#define IN_RANGE(x, min, max)   ((x)>=(min) && (x)<=(max))
#define CLAMP_RANGE(x, min, max) x = (x<(min)) ? min : ((x>(max)) ? max : x )
#define MIN(a, b) ((a)<=(b) ? (a) : (b))

int main(int hart_id)
{

#ifndef USE_RISCV_VECTOR
    if (hart_id == 0) run();
#else
    run_vector(hart_id);
#endif

#if NR_CORES > 1
    sync_barrier(); // all writes done; next t may read
#endif

}

#ifndef USE_RISCV_VECTOR

void run()
{
    int min;
    int *src,*dst, *temp;

    printf("NUMBER OF RUNS: %d\n",NUM_RUNS);
    long long start = get_time();

    for (int j=0; j<NUM_RUNS; j++) {
        src = new int[cols];
        for (int x = 0; x < cols; x++){
            result[x] = wall[x];
        }

        dst = result;
        for (int t = 0; t < rows-1; t++) {
            temp = src;
            src = dst;
            dst = temp;
            for(int n = 0; n < cols; n++){
              min = src[n];
              if (n > 0)
                min = MIN(min, src[n-1]);
              if (n < cols-1)
                min = MIN(min, src[n+1]);
              dst[n] = wall[(t+1)*cols + n]+min;
            }
        }
    }

    if(compare(cols, dst, reference)){
        printf("Verification failed!\n");
    } else {
        printf("Verification passed!\n");
    }

#ifdef RESULT_PRINT
    output_print(dst, cols);
#endif  // RESULT_PRINT

}

#else // USE_RISCV_VECTOR

void run_vector(int hart_id)
{
    int *dst;

    // Per-core column slice: each core owns [core_start, core_end)
    size_t cols_per_core = cols / NR_CORES;
    size_t core_start    = hart_id * cols_per_core;
    size_t core_end      = (hart_id == NR_CORES - 1) ? cols : core_start + cols_per_core;

    if (hart_id == 0)
        printf("NUMBER OF RUNS: %d rows: %d cols: %d L=%d C=%d cores=%d\n",
               NUM_RUNS, rows, cols, NR_LANES, NR_CLUSTERS, NR_CORES);

#if NR_CORES > 1
    sync_barrier();
#endif

#ifdef INTRINSICS
    for (int j=0; j<NUM_RUNS; j++) {
        // Each core initialises its own column slice of result[]
        for (size_t x = core_start; x < core_end; x++)
            result[x] = wall[x];
        dst = result;

#if NR_CORES > 1
        sync_barrier(); // all init writes done before any t-loop reads
#endif

        if (hart_id == 0)
            start_timer();

        size_t gvl;

        _MMR_i32    xSrc_slideup;
        _MMR_i32    xSrc_slidedown;
        _MMR_i32    xSrc;
        _MMR_i32    xNextrow;

        int aux, aux2;

        for (size_t t = 0; t < rows-1; t++)
        {
            // Pre-read cross-core boundary scalars BEFORE any core writes
            aux = (hart_id == 0) ? dst[0] : dst[core_start - 1];
            int right_boundary = (hart_id == NR_CORES - 1) ? dst[core_end - 1] : dst[core_end];

#if NR_CORES > 1
            sync_barrier(); // all boundaries captured; safe to start writing
#endif

            for(size_t n = core_start; n < core_end; n = n + gvl)
            {
                gvl = __riscv_vsetvl_e32m1(core_end - n);
                xNextrow = _MM_LOAD_i32(&dst[n],gvl);
                xSrc = xNextrow;
                aux2 = (n + gvl >= core_end) ? right_boundary : dst[n + gvl];
                xSrc_slideup = _MM_VSLIDE1UP_i32(xSrc,aux,gvl);
                xSrc_slidedown = _MM_VSLIDE1DOWN_i32(xSrc,aux2,gvl);
                xSrc = _MM_MIN_i32(xSrc,xSrc_slideup,gvl);
                xSrc = _MM_MIN_i32(xSrc,xSrc_slidedown,gvl);
                xNextrow = _MM_LOAD_i32(&wall[(t+1)*cols + n],gvl);
                xNextrow = _MM_ADD_i32(xNextrow,xSrc,gvl);
                aux = dst[n + gvl - 1];
                _MM_STORE_i32(&dst[n],xNextrow,gvl);
            }

#if NR_CORES > 1
            sync_barrier(); // all writes done; next t may read
#endif
        }
    }
#else // INTRINSICS
    for (int j=0; j<NUM_RUNS; j++) {
        // Each core initialises its own column slice of result[]
        for (size_t x = core_start; x < core_end; x++)
            result[x] = wall[x];
        dst = result;

#if NR_CORES > 1
        sync_barrier(); // all init writes done before any t-loop reads
#endif

        if (hart_id == 0)
            start_timer();

        size_t gvl;

        int aux, aux2;

        for (size_t t = 0; t < rows-1; t++)
        {
            // Pre-read cross-core boundary scalars BEFORE any core writes
            aux = (hart_id == 0) ? dst[0] : dst[core_start - 1];
            int right_boundary = (hart_id == NR_CORES - 1) ? dst[core_end - 1] : dst[core_end];
            for(size_t n = core_start; n < core_end; n = n + gvl)
            {
                asm volatile ("vsetvli %0, %1, e32, m8, ta, ma" : "=r"(gvl) : "r"(core_end - n));
                if (!((t > 0) && (gvl == (core_end - core_start))))
                    asm volatile ("vle32.v v0, (%0)"::"r"(&dst[n]));
                aux2 = (n + gvl >= core_end) ? right_boundary : dst[n + gvl];
                asm volatile ("vle32.v v24, (%0)"::"r"(&wall[(t+1)*cols + n]));
                asm volatile ("vslide1up.vx v16, v0, %0"::"r"(aux));
                asm volatile ("vmin.vv v0, v0, v16");
                asm volatile ("vslide1down.vx v8, v0, %0"::"r"(aux2));
                asm volatile ("vmin.vv v0, v0, v8");
                asm volatile ("vadd.vv v0, v0, v24");
                aux = dst[n + gvl - 1];
                asm volatile ("vse32.v v0, (%0)"::"r"(&dst[n]));
            }

#if NR_CORES > 1
            sync_barrier(); // all writes done; next t may read
#endif
        }
    }
#endif // INTRINSICS

    if (hart_id == 0) {
        stop_timer();
        int64_t cycles = get_timer();
        int64_t total_ops = (int64_t)(rows-1)*cols*3; // 3 ops/element: 2x MIN + 1x ADD
        int64_t ops_per_cycle = 2 * NR_LANES * NR_CLUSTERS * NR_CORES; // all cores contribute
        int64_t theoretical_cycles = (total_ops + ops_per_cycle - 1) / ops_per_cycle;
        float utilization = 100.0 * (float)theoretical_cycles/(float)cycles;
        if(compare(cols, dst, reference)){
            printf("Verification failed!\n");
        } else {
            printf("Verification passed!\n[sw-cycles]=%ld, util:%f%%\n", cycles, utilization);
        }
    }

#ifdef RESULT_PRINT
    if (hart_id == 0)
        output_print(dst, cols);
#endif // RESULT_PRINT

}
#endif // USE_RISCV_VECTOR

void output_print(int *dst, int cols) {

    for (int j = 0; j < cols; j++){
        printf("%d ",reference[j]) ;
    }
        printf("\n") ;
}

bool compare( int cols, int *result, int *reference){

    bool error = 0;
    for (int j = 0; j < cols; j++){
        if(result[j] != reference[j]){
            error = 1;
        }
        return error;
    }
    return error;
}
