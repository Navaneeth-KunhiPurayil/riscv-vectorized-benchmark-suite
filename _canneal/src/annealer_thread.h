// annealer_thread.h
//
// Created by Daniel Schwartz-Narbonne on 14/04/07.
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


#ifndef ANNEALER_THREAD_H
#define ANNEALER_THREAD_H

#if defined(ENABLE_THREADS) && (NR_CORES == 1)
#include <pthread.h>
#endif

#include "annealer_types.h"
#include "netlist.h"
#include "netlist_elem.h"
#include "rng.h"

extern "C" int canneal_keep_going_global_flag;

// Maximum vector mask size for RISC-V operations (compile-time constant)
#define MAX_MASK_SIZE 1024

class annealer_thread 
{
public:
	enum move_decision_t{
		move_decision_accepted_good,
		move_decision_accepted_bad,
		move_decision_rejected
	};

	annealer_thread()
	:_netlist(NULL),
	_moves_per_thread_temp(0),
	_start_temp(0),
	_number_temp_steps(0)
	{
#ifdef USE_RISCV_VECTOR
		for(int i = 0; i < MAX_MASK_SIZE; i++) {
			mask[i] = 0x55555555;
		}
#endif
	};

	void init(
		netlist* netlist,
		int nthreads,
		int swaps_per_temp,
		int start_temp,
		int number_temp_steps
	)
	{
		_netlist = netlist;
		__atomic_store_n(&canneal_keep_going_global_flag, 1, __ATOMIC_RELAXED);
		_moves_per_thread_temp = swaps_per_temp/nthreads;
		_start_temp = start_temp;
		_number_temp_steps = number_temp_steps;
		assert(_netlist != NULL);
#if defined(ENABLE_THREADS) && (NR_CORES == 1)
		pthread_barrier_init(&_barrier, NULL, nthreads);
#endif
	};

	annealer_thread(
		netlist* netlist,
		int nthreads,
		int swaps_per_temp,
		int start_temp,
		int number_temp_steps
	)
	:annealer_thread()
	{
		init(netlist, nthreads, swaps_per_temp, start_temp, number_temp_steps);
	};
	
	~annealer_thread() {
#if defined(ENABLE_THREADS) && (NR_CORES == 1)
		pthread_barrier_destroy(&_barrier);
#endif
	}					
	void Run(int hart_id);
					
protected:
	move_decision_t accept_move(routing_cost_t delta_cost, double T, Rng* rng);

#ifdef USE_RISCV_VECTOR
	routing_cost_t calculate_delta_routing_cost_vector(netlist_elem* a, netlist_elem* b, int hart_id/*,__epi_2xi1  xMask*/);
#else // !USE_RISCV_VECTOR
	routing_cost_t calculate_delta_routing_cost(netlist_elem* a, netlist_elem* b);
#endif // !USE_RISCV_VECTOR

	bool keep_going(int temp_steps_completed, int accepted_good_moves, int accepted_bad_moves);

protected:
	netlist* _netlist;		
	int _moves_per_thread_temp;
	int _start_temp;
	int _number_temp_steps;
#ifdef USE_RISCV_VECTOR
	int mask[MAX_MASK_SIZE]; // C-style fixed-size array, initialized in constructor
#endif // !USE_RISCV_VECTOR

#if defined(ENABLE_THREADS) && (NR_CORES == 1)
	pthread_barrier_t _barrier;
#endif
};

#endif
