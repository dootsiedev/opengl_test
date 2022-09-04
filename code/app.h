#pragma once

#include "cvar.h"

#include <SDL2/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
const char* emscripten_result_to_string(EMSCRIPTEN_RESULT result);
const char* emscripten_event_type_to_string(int eventType);
#endif

// Note this is not RAII safe
struct App_Info
{
	SDL_GLContext gl_context;
	SDL_Window* window;
};

NDSERR bool app_init(App_Info& app);
NDSERR bool app_destroy(App_Info& app);

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
