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

bool app_init(App_Info& app)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		serrf("SDL_Init Error: %s", SDL_GetError());
		return false;
	}

    // this is started for some reason...
    SDL_StopTextInput();

#define SDL_CHECK(x)                                  \
	do                                                \
	{                                                 \
		if((x) < 0)                                   \
		{                                             \
			slogf(#x " Warning: %s", SDL_GetError()); \
		}                                             \
	} while(0)

	// SDL_CHECK(SDL_SetHint("SDL_HINT_MOUSE_RELATIVE_MODE_WARP", "1"));
	//SDL_CHECK(SDL_SetHint("SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH", "1"));
#ifdef IME_TEXTEDIT_EXT
    // seems like this only works for wayland ATM.
	SDL_CHECK(SDL_SetHint("SDL_HINT_IME_SUPPORT_EXTENDED_TEXT", "1"));
#endif
	// SDL_CHECK(SDL_SetHint("SDL_HINT_IME_SHOW_UI", "1"));

	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES));

	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8));
	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8));

	int gl_context_flags = 0;

	if(cv_debug_opengl.data == 1)
	{
		gl_context_flags = SDL_GL_CONTEXT_DEBUG_FLAG;
	}

	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, gl_context_flags));

	// SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG |
	// SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG));
	// SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1));

#undef SDL_CHECK

	Uint32 fullscreen_mode = cv_fullscreen.data == 1 ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;

	app.window = SDL_CreateWindow(
		"A Window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		cv_screen_width.data,
		cv_screen_height.data,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | fullscreen_mode);
	if(app.window == NULL)
	{
		serrf("SDL_CreateWindow Error: %s", SDL_GetError());
		return false;
	}

	// clear previous SDL errors because we depend on checking it.
	SDL_ClearError();

	app.gl_context = SDL_GL_CreateContext(app.window);
	if(app.gl_context == NULL)
	{
		serrf("SDL_GL_CreateContext(): %s\n", SDL_GetError());
		return false;
	}

	// this happens because bad context hint errors are made when the context is made.
	// but the context still gets made and the only way to check is by this.
	// this isn't documented, but I tested by requesting opengl 3 with only opengl 2.
	const char* possible_error = SDL_GetError();
	if(possible_error != NULL && possible_error[0] != '\0')
	{
		slogf("warning SDL_GL_CreateContext SDL Error: %s", possible_error);
	}

	// since the context is bound to this thread implicitly we can load the functions.
	// the functions are loaded globally into "ctx"
	if(!LoadGLContext(&ctx))
	{
		return false;
	}

	// check if context flags were set.
	gl_context_flags = 0;
	if(SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &gl_context_flags) < 0)
	{
		slogf("SDL_GL_CONTEXT_FLAGS Warning: %s", SDL_GetError());
	}

	if(cv_debug_opengl.data == 1 && cv_has_GL_KHR_debug.data == 1)
	{
		if((gl_context_flags & SDL_GL_CONTEXT_DEBUG_FLAG) == 0)
		{
			slog("warning: SDL_GL_CONTEXT_DEBUG_FLAG failed to set\n");
		}
    }

	// vsync
	if(SDL_GL_SetSwapInterval(cv_vsync.data) != 0)
	{
		slogf("Warning: SDL_GL_SetSwapInterval(): %s\n", SDL_GetError());
	}

	return true;
}

void app_destroy(App_Info& app)
{
	if(app.gl_context != NULL)
	{
		ASSERT(SDL_GL_GetCurrentContext() == app.gl_context);
		SDL_GL_DeleteContext(app.gl_context);
		app.gl_context = NULL;
		memset(&ctx, 0, sizeof(ctx));
	}

	if(app.window != NULL)
	{
		SDL_DestroyWindow(app.window);
		app.window = NULL;
	}

	SDL_Quit();
}


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