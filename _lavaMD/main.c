//========================================================================================================================================================================================================200
//======================================================================================================================================================150
//====================================================================================================100
//==================================================50

//========================================================================================================================================================================================================200
//	UPDATE
//========================================================================================================================================================================================================200

//	14 APR 2011 Lukasz G. Szafaryn

//========================================================================================================================================================================================================200
//	DEFINE/INCLUDE
//========================================================================================================================================================================================================200

//======================================================================================================================================================150
//	LIBRARIES
//======================================================================================================================================================150

#include <stdio.h>					// (in path known to compiler)			needed by printf
#include <stdlib.h>					// (in path known to compiler)			needed by malloc
#include <stdbool.h>				// (in path known to compiler)			needed by true/false
#include <string.h>

// #include <time.h>
// #include <sys/time.h>

//======================================================================================================================================================150
//	UTILITIES
//======================================================================================================================================================150

//#include "./util/timer/timer.h"			// (in path specified here)
#include "./util/num/num.h"				// (in path specified here)

#include "common/riscv_util.h"
//======================================================================================================================================================150
//	MAIN FUNCTION HEADER
//======================================================================================================================================================150

#include "./main.h"						// (in the current directory)

//======================================================================================================================================================150
//	KERNEL
//======================================================================================================================================================150

#ifdef USE_RISCV_VECTOR
#include "kernel/kernel_vector.h"				// (in library path specified here)
#else
#include "kernel/kernel_cpu.h"				// (in library path specified here)
#endif

#include "printf.h"
#include "runtime.h"

// #define PRINT_RESULT

//========================================================================================================================================================================================================200
//	MAIN FUNCTION
//========================================================================================================================================================================================================200
extern int cores;
extern int boxes1d;

int main(int hart_id)
{

	//======================================================================================================================================================150
	//	CPU/MCPU VARIABLES
	//======================================================================================================================================================150

	// timer
	long long time0;

	// time0 = get_time();

	// timer
	// long long time1;
	long long time2;
	long long time3;
	long long time4;
	long long time5;
	long long time6;
	long long time7;

	// counters
	int i, j, k, l, m, n;

	// system memory
	static par_str par_cpu;
	static dim_str dim_cpu;
	static box_str* box_cpu;
	static FOUR_VECTOR* rv_cpu;
	static fp* qv_cpu;
	static FOUR_VECTOR* fv_cpu;
	int nh;

	// Per-core kernel arguments (set up by core 0)
	static dim_str per_core_dim[NR_CORES];
	static box_str* per_core_box[NR_CORES];

	// time1 = get_time();

	if (hart_id == 0) {

	//======================================================================================================================================================150
	//	CHECK INPUT ARGUMENTS
	//======================================================================================================================================================150

	// assing default values
	dim_cpu.cores_arg = cores;
	dim_cpu.boxes1d_arg = boxes1d;

	// Print configuration
	printf("Configuration used: cores = %d, boxes1d = %d, NUMBER_PAR_PER_BOX = %d\n", dim_cpu.cores_arg, dim_cpu.boxes1d_arg, NUMBER_PAR_PER_BOX);

	//======================================================================================================================================================150
	//	INPUTS
	//======================================================================================================================================================150

	par_cpu.alpha = 0.5;

	//======================================================================================================================================================150
	//	DIMENSIONS
	//======================================================================================================================================================150

	// total number of boxes
	dim_cpu.number_boxes = dim_cpu.boxes1d_arg * dim_cpu.boxes1d_arg * dim_cpu.boxes1d_arg;

	// how many particles space has in each direction
	dim_cpu.space_elem = dim_cpu.number_boxes * NUMBER_PAR_PER_BOX;
	dim_cpu.space_mem = dim_cpu.space_elem * sizeof(FOUR_VECTOR);
	dim_cpu.space_mem2 = dim_cpu.space_elem * sizeof(fp);

	// box array
	dim_cpu.box_mem = dim_cpu.number_boxes * sizeof(box_str);

	//======================================================================================================================================================150
	//	SYSTEM MEMORY
	//======================================================================================================================================================150

	//====================================================================================================100
	//	BOX
	//====================================================================================================100

	// allocate boxes
	box_cpu = (box_str*)baremetal_malloc(dim_cpu.box_mem);

	// initialize number of home boxes
	nh = 0;

	// home boxes in z direction
	for(i=0; i<dim_cpu.boxes1d_arg; i++){
		// home boxes in y direction
		for(j=0; j<dim_cpu.boxes1d_arg; j++){
			// home boxes in x direction
			for(k=0; k<dim_cpu.boxes1d_arg; k++){

				// current home box
				box_cpu[nh].x = k;
				box_cpu[nh].y = j;
				box_cpu[nh].z = i;
				box_cpu[nh].number = nh;
				box_cpu[nh].offset = nh * NUMBER_PAR_PER_BOX;

				// initialize number of neighbor boxes
				box_cpu[nh].nn = 0;

				// neighbor boxes in z direction
				for(l=-1; l<2; l++){
					// neighbor boxes in y direction
					for(m=-1; m<2; m++){
						// neighbor boxes in x direction
						for(n=-1; n<2; n++){

							// check if (this neighbor exists) and (it is not the same as home box)
							if(		(((i+l)>=0 && (j+m)>=0 && (k+n)>=0)==true && ((i+l)<dim_cpu.boxes1d_arg && (j+m)<dim_cpu.boxes1d_arg && (k+n)<dim_cpu.boxes1d_arg)==true)	&&
									(l==0 && m==0 && n==0)==false	){

								// current neighbor box
								box_cpu[nh].nei[box_cpu[nh].nn].x = (k+n);
								box_cpu[nh].nei[box_cpu[nh].nn].y = (j+m);
								box_cpu[nh].nei[box_cpu[nh].nn].z = (i+l);
								box_cpu[nh].nei[box_cpu[nh].nn].number =	(box_cpu[nh].nei[box_cpu[nh].nn].z * dim_cpu.boxes1d_arg * dim_cpu.boxes1d_arg) +
																			(box_cpu[nh].nei[box_cpu[nh].nn].y * dim_cpu.boxes1d_arg) +
																			 box_cpu[nh].nei[box_cpu[nh].nn].x;
								box_cpu[nh].nei[box_cpu[nh].nn].offset = box_cpu[nh].nei[box_cpu[nh].nn].number * NUMBER_PAR_PER_BOX;

								// increment neighbor box
								box_cpu[nh].nn = box_cpu[nh].nn + 1;

							}

						} // neighbor boxes in x direction
					} // neighbor boxes in y direction
				} // neighbor boxes in z direction

				// increment home box
				nh = nh + 1;

			} // home boxes in x direction
		} // home boxes in y direction
	} // home boxes in z direction

	//====================================================================================================100
	//	PARAMETERS, DISTANCE, CHARGE AND FORCE
	//====================================================================================================100

	// input (distances)
	rv_cpu = (FOUR_VECTOR*)baremetal_malloc(dim_cpu.space_mem);
	for(i=0; i<dim_cpu.space_elem; i=i+1){
		rv_cpu[i].v = (rand()%10 + 1) / 10.0;			// get a number in the range 0.1 - 1.0
		rv_cpu[i].x = (rand()%10 + 1) / 10.0;			// get a number in the range 0.1 - 1.0
		rv_cpu[i].y = (rand()%10 + 1) / 10.0;			// get a number in the range 0.1 - 1.0
		rv_cpu[i].z = (rand()%10 + 1) / 10.0;			// get a number in the range 0.1 - 1.0
	}

	// input (charge)
	qv_cpu = (fp*)baremetal_malloc(dim_cpu.space_mem2);
	for(i=0; i<dim_cpu.space_elem; i=i+1){
		qv_cpu[i] = (rand()%10 + 1) / 10.0;			// get a number in the range 0.1 - 1.0
	}

	// output (forces)
	fv_cpu = (FOUR_VECTOR*)baremetal_malloc(dim_cpu.space_mem);
	for(i=0; i<dim_cpu.space_elem; i=i+1){
		fv_cpu[i].v = 0;								// set to 0, because kernels keeps adding to initial value
		fv_cpu[i].x = 0;								// set to 0, because kernels keeps adding to initial value
		fv_cpu[i].y = 0;								// set to 0, because kernels keeps adding to initial value
		fv_cpu[i].z = 0;								// set to 0, because kernels keeps adding to initial value
	}

	//======================================================================================================================================================150
	//	PER-CORE BOX PARTITIONING
	//======================================================================================================================================================150

	long boxes_per_core = (dim_cpu.number_boxes + NR_CORES - 1) / NR_CORES;
	for (int c = 0; c < NR_CORES; c++) {
		long start_box = (long)c * boxes_per_core;
		long end_box   = start_box + boxes_per_core;
		if (end_box > dim_cpu.number_boxes)
			end_box = dim_cpu.number_boxes;
		long my_num_boxes = end_box - start_box;

		// Allocate a full-size copy of box_cpu for this core.
		// We pass (box_c + start_box) to kernel_cpu so that:
		//   home box:  box_ptr[l]       = box_c[start_box + l]         (correct)
		//   neighbor:  box_ptr[relative] = box_c[start_box + relative]  (correct)
		// where relative = absolute - start_box, via pointer arithmetic.
		box_str* box_c = (box_str*)baremetal_malloc(dim_cpu.box_mem);
		memcpy(box_c, box_cpu, dim_cpu.box_mem);

		// Remap nei.number in this core's home boxes from absolute to relative.
		for (long b = 0; b < my_num_boxes; b++) {
			box_str* hb = &box_c[start_box + b];
			for (int ni = 0; ni < hb->nn; ni++) {
				hb->nei[ni].number -= (int)start_box;
			}
		}

		per_core_box[c] = box_c + start_box;
		per_core_dim[c] = dim_cpu;
		per_core_dim[c].number_boxes = my_num_boxes;
	}

	//======================================================================================================================================================150
	//	KERNEL
	//======================================================================================================================================================150

	//====================================================================================================100
	//	CPU/MCPU
	//====================================================================================================100

	} // end if (hart_id == 0) -- setup and timer start

	sync_barrier();

	if (hart_id == 0)
		start_timer();

	kernel_cpu(	par_cpu,
				per_core_dim[hart_id],
				per_core_box[hart_id],
				rv_cpu,
				qv_cpu,
				fv_cpu);

	if (hart_id == 0)
		stop_timer();

	sync_barrier();

	if (hart_id == 0) {

		int64_t cycles = get_timer();
#ifdef MEASURE_PERFORMANCE
		int64_t total_ops = 16 * NUMBER_PAR_PER_BOX * 36; // 36 vec. insn. within inner most loop
#else
		int64_t total_ops = NUMBER_PAR_PER_BOX * NUMBER_PAR_PER_BOX * 36; // 36 vec. insn. within inner most loop
#endif
		int64_t ops_per_cycle = 2 * NR_LANES * NR_CLUSTERS * NR_CORES; // 2x 32-bit ops per lane
		int64_t theoretical_cycles = (total_ops + ops_per_cycle - 1) / ops_per_cycle; // Ceiling division
		float utilization = 100.0 * (float)theoretical_cycles/(float)cycles;

		printf("Finished kernel [sw-cycles]=%ld util:%f%%\n", cycles, utilization);

#ifdef PRINT_RESULT
		for(i=0; i<dim_cpu.space_elem; i=i+1){
			printf("%f, %f, %f, %f\n", fv_cpu[i].v, fv_cpu[i].x, fv_cpu[i].y, fv_cpu[i].z);
		}
#endif

	} // end if (hart_id == 0) -- output

	return 0;																					// always returns 0.0
}
