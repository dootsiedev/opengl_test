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
extern cvar_int cv_debug_opengl;

class cvar_fullscreen : public cvar_int
{
public:
	cvar_fullscreen();
	NDSERR bool cvar_read(const char* buffer) override;
};
extern cvar_fullscreen cv_fullscreen;

class cvar_vysnc : public cvar_int
{
public:
	cvar_vysnc();
	NDSERR bool cvar_read(const char* buffer) override;
};

extern cvar_vysnc cv_vsync;