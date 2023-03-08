/* Copyright (C) 2023 Elijah Day

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the “Software”), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include "SDL2/SDL.h"
#include "SDL2/SDL_net.h"
#include "sodium.h"
#include "src/err.h"
#include "src/init.h"
#include "stdbool.h"

bool init_libsdl(void)
{
	bool init_success = true;
	
	/* 0 in SDL_Init for no init flags (it's only needed so that SDL_net can
	be init) */
	if(SDL_Init(0) != 0)
	{
		print_libsdl_err("SDL_Init");
		init_success = false;
	}
	
	return init_success;
}

bool init_libsdlnet(void)
{
	bool init_success = true;
	
	if(SDLNet_Init() != 0)
	{
		print_libsdl_err("SDLNet_Init");
		init_success = false;
	}
	
	return init_success;
}

bool init_libsodium(void)
{
	bool init_success = true;
	
	if(sodium_init() == -1)
	{
		print_err("sodium_init", "Could not init Sodium");
		init_success = false;
	}
	
	return init_success;
}
