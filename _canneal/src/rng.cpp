// rng.cpp
//
// Created by Daniel Schwartz-Narbonne on 25/04/07.
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


#include "rng.h"
#include <stdlib.h>

static unsigned long mix_seed(unsigned long seed)
{
	seed ^= seed >> 16;
	seed *= 0x7feb352dUL;
	seed ^= seed >> 15;
	seed *= 0x846ca68bUL;
	seed ^= seed >> 16;
	return seed ? seed : 1;
}

Rng::Rng()
{
	seed(1);
}

Rng::Rng(unsigned long seed_value)
{
	seed(seed_value);
}

void Rng::seed(unsigned long seed_value)
{
	_state = mix_seed(seed_value);
}

long Rng::rand(int max)
{
	return rand() % max;
}


long Rng::rand()
{
	_state ^= _state << 13;
	_state ^= _state >> 17;
	_state ^= _state << 5;
	return (long)(_state & 0x7fffffffUL);
}

double Rng::drand()
{
	return (double)rand() / 2147483648.0;
}
