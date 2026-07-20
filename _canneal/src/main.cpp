// main.cpp
//
// Created by Daniel Schwartz-Narbonne on 13/04/07.
// Modified by Christian Bienia
//
// Copyright 2007-2008 Princeton University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

/*************************************************************************
* RISC-V Vectorized Version
* Author: Cristóbal Ramírez Lazo
* email: cristobal.ramirez@bsc.es
* Barcelona Supercomputing Center (2020)
*************************************************************************/

#ifdef USE_RISCV_VECTOR
#include <riscv_vector.h>
#include "common/vector_defines.h"
#endif

#ifdef ENABLE_PARSEC_HOOKS
#include <hooks.h>
#endif

#include "annealer_types.h"
#include "annealer_thread.h"
#include "netlist.h"
#include "rng.h"

#include <stdlib.h>
#include "printf.h"
#include "runtime.h"

// Keep large benchmark state out of the per-hart stack.
static netlist shared_netlist;
static annealer_thread a_threads[NR_CORES];

// Static compile-time configuration for CANNEAL
#define USE_COMPILED_NETLIST
#define CANNEAL_NUM_THREADS (NR_CORES)
#define CANNEAL_SWAPS_PER_TEMP 100
#define CANNEAL_START_TEMP 300     // Lesser temperature more likely a bad move will be rejected
#define CANNEAL_NUM_TEMP_STEPS 10  // -1 means run until convergence

void* entry_pt(void*);

int main (int hart_id) {

	// Baremetal execution - no stdout or system time available

#if NR_CORES == 1
	if (hart_id != 0) {
		while (1)
			;
	}
#endif
	
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_bench_begin(__parsec_canneal);
#endif

	// Use statically defined configuration values (no command-line parsing)
	int num_threads = CANNEAL_NUM_THREADS;

	// Use static configuration values
	int swaps_per_temp = CANNEAL_SWAPS_PER_TEMP;
	int start_temp = CANNEAL_START_TEMP;
	int number_temp_steps = CANNEAL_NUM_TEMP_STEPS;

	if (hart_id == 0) {
		srand(0);
		shared_netlist.init(true);
	}
#if NR_CORES > 1
	sync_barrier();
#endif

	if (hart_id == 0) {
		printf(" CANNEAL Configuration:\n");
		printf("  num_cores: %d\n", num_threads);
		printf("  swaps_per_temp: %d\n", swaps_per_temp);
		printf("  start_temp: %d\n", start_temp);
		printf("  number_temp_steps: %d\n", number_temp_steps);
		printf("  Lanes=%d\n", NR_LANES);
		printf("  Clusters=%d\n", NR_CLUSTERS);
		printf("  shared_netlist_size: %lu addr:%p\n", (unsigned long)sizeof(shared_netlist), (void*)&shared_netlist);
	}
	a_threads[hart_id].init(&shared_netlist, num_threads, swaps_per_temp, start_temp, number_temp_steps);
	
#ifdef ENABLE_PARSEC_HOOKS
	__parsec_roi_begin();
#endif

#if NR_CORES > 1
	sync_barrier();
	if (hart_id == 0) {
		start_timer();
	}
#else
	start_timer();
#endif

	a_threads[hart_id].Run(hart_id);

#if NR_CORES > 1
	sync_barrier();
	if (hart_id == 0) {
		stop_timer();
	}
#else
	stop_timer();
#endif

#ifdef ENABLE_PARSEC_HOOKS
	__parsec_roi_end();
#endif

#ifdef USE_RISCV_VECTOR
	extern unsigned long swap_cost_vector_calls[NR_CORES];

	if (hart_id == 0) {
		printf("Total execution time [sw-cycles]: %ld\n", get_timer());
	}

#if NR_CORES > 1
	for (int core = 0; core < num_threads; core++) {
		if (hart_id == core) {
			printf("hart %d swap_cost_vector calls: %lu\n", hart_id, swap_cost_vector_calls[hart_id]);
		}
		sync_barrier();
	}
#else
	printf("hart %d swap_cost_vector calls: %lu\n", hart_id, swap_cost_vector_calls[hart_id]);
#endif
#endif

	return 0;
	
}
