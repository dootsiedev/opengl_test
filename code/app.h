#pragma once

#include "cvar.h"

#include <SDL2/SDL.h>

struct App_Info
{
	SDL_GLContext gl_context;
	SDL_Window* window;
};

extern App_Info g_app;

extern cvar_int cv_screen_width;
extern cvar_int cv_screen_height;
extern cvar_int cv_fullscreen;