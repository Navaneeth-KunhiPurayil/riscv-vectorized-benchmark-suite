// netlist.cpp
//
// Created by Daniel Schwartz-Narbonne on 14/04/07.
//
// Copyright 2007 Princeton University
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


#include "location_t.h"
#include "netlist.h"
#include "netlist_elem.h"
#include "rng.h"

# include <string.h>
#include <assert.h>

#include "printf.h"

// External symbols from compiled assembly netlist data
#ifdef USE_COMPILED_NETLIST
extern const unsigned long compiled_num_elements_data[];
extern const unsigned long compiled_max_x_data[];
extern const unsigned long compiled_max_y_data[];
extern const char* compiled_element_names[];
extern const unsigned long compiled_element_types[];
extern const char* compiled_connections[];
extern const unsigned long compiled_connection_offsets[];
extern const unsigned long compiled_connection_counts[];

// Convenience macros to access metadata from arrays
#define compiled_num_elements (compiled_num_elements_data[0])
#define compiled_max_x (compiled_max_x_data[0])
#define compiled_max_y (compiled_max_y_data[0])
#endif

using namespace std;

// Global variable to track element allocation across different netlist instances
static unsigned g_unused_elem = 0;

void netlist::release(netlist_elem* elem)
{
	return;
}


//*****************************************************************************************
//not thread safe, tho i could make it so if i needed to
//only look at the non_blank elements:  this saves some time
//*****************************************************************************************
routing_cost_t netlist::total_routing_cost()
{
	routing_cost_t rval = 0;
	for (unsigned i = 0; i < _num_elements; i++) {
		netlist_elem* elem = &_elements[i];
		if (elem->present_loc.Get() != NULL) {
			rval += elem->routing_cost_given_loc(*(elem->present_loc.Get()));
		}
	}
	return rval / 2; //since routing_cost calculates both input and output routing, we have double counted
}



//*****************************************************************************************
// just use a simple shuffle algorithm
//*****************************************************************************************
void netlist::shuffle(Rng* rng)
{
	for (int i = 0; i < _max_x * _max_y * 1000; i++){
		netlist_elem *a, *b;
		get_random_pair(&a, &b, rng);
		swap_locations(a, b);
	}
}


//*****************************************************************************************
//  SYNC need chris' atomic swap algorithm
//*****************************************************************************************
void netlist::swap_locations(netlist_elem* elem_a, netlist_elem* elem_b)
{
	//and swap the locations stored in the actual netlist_elem
	elem_a->present_loc.Swap(elem_b->present_loc);
}


//*****************************************************************************************
//returns an elemment that is different from the different_from element
// if different_from == NO_MATCHING_ELEMENT then returns any element
//*****************************************************************************************
netlist_elem* netlist::get_random_element(long* elem_id, long different_from, Rng* rng)
{
	printf("Getting random element different from ID: %ld\n", different_from);
	long id = rng->rand(_chip_size);
	printf("Random element ID: %ld\n", id);
	netlist_elem* elem = &(_elements[id]);
	
	//loop until we get a non duplicate element
	//-1 is not a possible element, will never enter this loop in that case
	//if it doesn't work, try a new one
	while (id == different_from){ 
		id = rng->rand(_chip_size);
		printf("Random element ID: %ld\n", id);
		elem = &(_elements[id]);
	}
	*elem_id=id;
	return elem;
}


//*****************************************************************************************
//assumption: will return elements a, b which we can get a valid lock on
//*****************************************************************************************
void netlist::get_random_pair(netlist_elem** a, netlist_elem** b, Rng* rng)
{
	//get a random element
	long id_a = rng->rand(_chip_size);
	netlist_elem* elem_a = &(_elements[id_a]);
	
	//now do the same for b
	long id_b = rng->rand(_chip_size);
	netlist_elem* elem_b = &(_elements[id_b]);

	//keep trying new elements until we get one that works
	//get required locks automatically rolls back if it fails
	//keep going until we get
	while (id_b == id_a){ //no duplicate elements
		//if it doesn't work, try a new one
		id_b = rng->rand(_chip_size);
		elem_b = &(_elements[id_b]);
	}

	*a = elem_a;
	*b = elem_b;
	return;
}

//*****************************************************************************************
//  No longer easy to implement
//*****************************************************************************************
netlist_elem* netlist::netlist_elem_from_loc(location_t& loc)
{
	assert(false);
	return NULL;
}

//*****************************************************************************************
//
//*****************************************************************************************
netlist_elem* netlist::netlist_elem_from_name(const char* name)
{
	return find_elem_by_name(name);
}

//*****************************************************************************************
// Linear search for element by name (replaces std::map functionality)
//*****************************************************************************************
netlist_elem* netlist::find_elem_by_name(const char* name)
{
	for (unsigned i = 0; i < g_unused_elem; i++) {
		if (strcmp(_elements[i].item_name, name) == 0) {
			return &_elements[i];
		}
	}
	return NULL;
}

//*****************************************************************************************
// Used in the ctor.  Since an element have fanin from an element that can occur both
// earlier and later in the input file, we must handle both cases
//*****************************************************************************************
netlist_elem* netlist::create_elem_if_necessary(const char* name)
{
	netlist_elem* rval;
	//check whether we already have a netlist element with that name
	rval = find_elem_by_name(name);
	if (rval == NULL) {
		//if not found, get one from the _elements pool
		rval = &_elements[g_unused_elem];
		strncpy(rval->item_name, name, MAX_ELEMENT_NAME_LENGTH - 1);
		rval->item_name[MAX_ELEMENT_NAME_LENGTH - 1] = '\0';
		g_unused_elem++;
	}
	return rval;
}

//*****************************************************************************************
// Initialize location grid - common code used by both constructors
//*****************************************************************************************
void netlist::initialize_locations()
{
	printf("Initializing locations...\n");
	unsigned i_elem = 0;
	for (int x = 0; x < _max_x; x++){
		for (int y = 0; y < _max_y; y++){
			location_t* loc = &_locations[x][y];
			loc->x = x;
			loc->y = y;

			// Initialize default values in all locations
			_elements[i_elem].present_loc.Set(loc);
			_elements[i_elem].fanin_count = 0;
			_elements[i_elem].fanout_count = 0;
			_elements[i_elem].fan_locs_count = 0;
			strncpy(_elements[i_elem].item_name, "empty", MAX_ELEMENT_NAME_LENGTH - 1);
			_elements[i_elem].item_name[MAX_ELEMENT_NAME_LENGTH - 1] = '\0';
			i_elem++;
		}//for (int y = 0; y < _max_y; y++)
	}//for (int x = 0; x < _max_x; x++)
}

//*****************************************************************************************
// Constructor using compiled static netlist data
// pass true to use compiled data (requires netlist_data.h to be included with USE_COMPILED_NETLIST)
//*****************************************************************************************
netlist::netlist(bool use_compiled_data)
{
	if (!use_compiled_data) {
		assert(false); // This constructor requires use_compiled_data=true
	}
	
#ifdef USE_COMPILED_NETLIST
	g_unused_elem = 0; // Reset global counter for compiled data loading
	
	// Set metadata from compiled data
	_num_elements = compiled_num_elements;
	_max_x = compiled_max_x;
	_max_y = compiled_max_y;
	_chip_size = _max_x * _max_y;
	assert(_num_elements < _chip_size);
	
	initialize_locations();
	
	// Initialize from compiled data
	initialize_from_compiled_data();
	
#else
	assert(false); // USE_COMPILED_NETLIST must be defined
#endif
}

//*****************************************************************************************
// Initialize netlist from compiled static arrays (stored in assembly)
// Accesses external symbols: compiled_element_names[], compiled_connections[], etc.
//*****************************************************************************************
void netlist::initialize_from_compiled_data()
{
	printf("Initializing compiled data...\n");
#ifdef USE_COMPILED_NETLIST
	// Create all elements with their names
	for (unsigned i = 0; i < _num_elements; i++) {
		// Element names are stored as pointers, dereference to get the actual string
		const char* name_ptr = compiled_element_names[i];
		netlist_elem* elem = create_elem_if_necessary(name_ptr);
		printf("Element %u: %s\n", i, elem->item_name);
	}
	
	// Add fanin/fanout connections
	for (unsigned i = 0; i < _num_elements; i++) {
		// Get element name
		const char* name_ptr = compiled_element_names[i];
		netlist_elem* present_elem = find_elem_by_name(name_ptr);
		
		// Get connections for this element
		unsigned conn_offset = compiled_connection_offsets[i];
		unsigned conn_count = compiled_connection_counts[i];
		
		// Process each fanin connection
		for (unsigned j = 0; j < conn_count; j++) {
			// Connections are stored as pointers to strings
			const char* fanin_ptr = compiled_connections[conn_offset + j];
			netlist_elem* fanin_elem = create_elem_if_necessary(fanin_ptr);
			
			// Add to fanin/fanout relationships (C-style array with bounds checking)
			if (present_elem->fanin_count < MAX_FANIN_PER_ELEM) {
				present_elem->fanin[present_elem->fanin_count++] = fanin_elem;
			}
			if (fanin_elem->fanout_count < MAX_FANOUT_PER_ELEM) {
				fanin_elem->fanout[fanin_elem->fanout_count++] = present_elem;
			}
			
#ifdef USE_RISCV_VECTOR
			unsigned long * fanin_location = (unsigned long *)&fanin_elem->present_loc;
			if (present_elem->fan_locs_count < MAX_FAN_LOCS_PER_ELEM) {
				present_elem->fan_locs[present_elem->fan_locs_count++] = fanin_location;
			}
			unsigned long * fanout_location = (unsigned long *)&present_elem->present_loc;
			if (fanin_elem->fan_locs_count < MAX_FAN_LOCS_PER_ELEM) {
				fanin_elem->fan_locs[fanin_elem->fan_locs_count++] = fanout_location;
			}
#endif
		}
	}
#else
	assert(false); // USE_COMPILED_NETLIST must be defined
#endif
}
