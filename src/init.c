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
