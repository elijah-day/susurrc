#include "SDL2/SDL.h"
#include "src/err.h"

void print_err(const char *func_name, const char *err_msg)
{
	printf("%s err: %s\n", func_name, err_msg);
}

void print_libsdl_err(const char *func_name)
{
	printf("%s err: %s\n", func_name, SDL_GetError());
}
