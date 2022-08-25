#include "global.h"
#include "app.h"

#include "opengles2/opengl_stuff.h"

// this is a very empty file, but app initialization is done in main.
App_Info g_app;

REGISTER_CVAR_INT(cv_screen_width, 640, "screen width in windowed mode", CVAR_T::STARTUP);
REGISTER_CVAR_INT(cv_screen_height, 480, "screen height in windowed mode", CVAR_T::STARTUP);

REGISTER_CVAR_INT(
	cv_debug_opengl,
	0,
	"0 = off, 1 = show detailed opengl errors, 2 = stacktrace per call",
	CVAR_T::STARTUP);

cvar_fullscreen cv_fullscreen;
cvar_vysnc cv_vsync;

cvar_fullscreen::cvar_fullscreen()
: cvar_int("cv_fullscreen", 0, "0 = windowed, 1 = fullscreen", CVAR_T::RUNTIME, __FILE__, __LINE__)
{
}
bool cvar_fullscreen::cvar_read(const char* buffer)
{
	bool ret = cvar_int::cvar_read(buffer);
	if(ret && g_app.window != NULL)
	{
		Uint32 fullscreen_mode = cv_fullscreen.data == 1 ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
		if(SDL_SetWindowFullscreen(g_app.window, fullscreen_mode) < 0)
		{
			slogf("info: SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
		}
	}
	return ret;
}

cvar_vysnc::cvar_vysnc()
: cvar_int("cv_vsync", 0, "0 (off), 1 (on), -1 (adaptive?)", CVAR_T::RUNTIME, __FILE__, __LINE__)
{
}

bool cvar_vysnc::cvar_read(const char* buffer)
{
	bool ret = cvar_int::cvar_read(buffer);
	if(ret && g_app.window != NULL)
	{
		if(SDL_GL_SetSwapInterval(data) != 0)
		{
			slogf("Warning: SDL_GL_SetSwapInterval(): %s\n", SDL_GetError());
		}
	}
	return ret;
}