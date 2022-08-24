#include "global.h"
#include "app.h"

// this is a very empty file, but app initialization is done in main.
App_Info g_app;

REGISTER_CVAR_INT(cv_screen_width, 640, "screen width in windowed mode", CVAR_T::STARTUP);
REGISTER_CVAR_INT(cv_screen_height, 480, "screen height in windowed mode", CVAR_T::STARTUP);
REGISTER_CVAR_INT(cv_fullscreen, 0, "0 = windowed, 1 = fullscreen", CVAR_T::STARTUP);

REGISTER_CVAR_INT(
	cv_debug_opengl,
	0,
	"0 = off, 1 = show detailed opengl errors, 2 = stacktrace per call",
	CVAR_T::STARTUP);