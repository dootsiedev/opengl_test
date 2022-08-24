#include "global.h"

#include "opengles2/gl3platform.h"
#include "opengles2/gl3.h"
#include "opengles2/gl2ext.h"
#include "opengles2/opengl_stuff.h"

#include "cvar.h"
#include "app.h"
#include "demo.h"
#include "debug_tools.h"



#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>

static REGISTER_CVAR_INT(cv_vsync, 0, "0 (off), 1 (on), -1 (adaptive?)", CVAR_T::STARTUP);


static const char* GetGLDebugSeverityKHR(GLenum severity)
{
#define GL_DEBUG_SEVERITY(x) \
	case(GL_DEBUG_SEVERITY_##x##_KHR): return (#x);
	switch(severity)
	{
		GL_DEBUG_SEVERITY(HIGH)
		GL_DEBUG_SEVERITY(MEDIUM)
		GL_DEBUG_SEVERITY(LOW)
		GL_DEBUG_SEVERITY(NOTIFICATION)
	}
#undef GL_DEBUG_SEVERITY
	return "UNKNOWN";
}

static const char* GetGLDebugTypeKHR(GLenum type)
{
#define GL_DEBUG_TYPE(x) \
	case(GL_DEBUG_TYPE_##x##_KHR): return (#x);
	switch(type)
	{
		GL_DEBUG_TYPE(DEPRECATED_BEHAVIOR)
		GL_DEBUG_TYPE(ERROR)
		GL_DEBUG_TYPE(MARKER)
		GL_DEBUG_TYPE(OTHER)
		GL_DEBUG_TYPE(PERFORMANCE)
		GL_DEBUG_TYPE(POP_GROUP)
		GL_DEBUG_TYPE(PUSH_GROUP)
		GL_DEBUG_TYPE(UNDEFINED_BEHAVIOR)
	}
#undef GL_DEBUG_TYPE
	return "UNKNOWN";
}

// from https://github.com/nvMcJohn/apitest
// I am keeping the link because I want to look at the code as a reference
// --------------------------------------------------------------------------------------------------------------------
static void ErrorCallback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* userParam)
{
	(void)userParam;
	(void)length;
	(void)source;
	(void)id;

	if(type != GL_DEBUG_TYPE_ERROR_KHR && type != GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR)
	{
		slogf(
			"\nGL CALLBACK: type = %s (0x%x), severity = %s (0x%x), message = %s\n",
			GetGLDebugTypeKHR(type),
			type,
            GetGLDebugSeverityKHR(severity),
			severity,
			message);
	}
	else
	{
		serrf(
			"\nGL CALLBACK: type = %s (0x%x), severity = %s (0x%x), message = %s\n",
			GetGLDebugTypeKHR(type),
			type,
            GetGLDebugSeverityKHR(severity),
			severity,
			message);

		static bool only_once = false;
		if(!only_once || cv_debug_opengl.data == 2.0)
		{
			only_once = true;
			std::string stack_message;
			debug_str_stacktrace(&stack_message, 0);
			serrf(
				"\nGL StackTrace:\n"
				"%s\n",
				stack_message.c_str());
		}
	}
}

static bool app_init(App_Info& app)
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

	if(cv_debug_opengl.data != 0)
	{
		gl_context_flags = SDL_GL_CONTEXT_DEBUG_FLAG;
	}

	SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, gl_context_flags));

	// SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG |
	// SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG));
	// SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1));

#undef SDL_CHECK

	app.window = SDL_CreateWindow(
		"A Window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		cv_screen_width.data,
		cv_screen_height.data,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
			(cv_fullscreen.data == 1 ? SDL_WINDOW_FULLSCREEN : 0));
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

	if(cv_debug_opengl.data != 0)
	{
		if(SDL_GL_ExtensionSupported("GL_KHR_debug") == SDL_FALSE)
		{
			slog("warning cv_opengl_debug: GL_KHR_debug unsupported\n");
		}

		if((gl_context_flags & SDL_GL_CONTEXT_DEBUG_FLAG) == 0)
		{
			slog("warning: SDL_GL_CONTEXT_DEBUG_FLAG failed to set\n");
		}
		else
		{
			ASSERT(ctx.glDebugMessageControl);
			ASSERT(ctx.glDebugMessageCallback);
			ctx.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			ctx.glDebugMessageCallback(ErrorCallback, nullptr);
			ctx.glEnable(GL_DEBUG_OUTPUT_KHR);
		}
	}

	// vsync
	if(SDL_GL_SetSwapInterval(cv_vsync.data) != 0)
	{
		slogf("Warning: SDL_GL_SetSwapInterval(): %s\n", SDL_GetError());
	}

	return true;
}

static void app_destroy(App_Info& app)
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

int main(int argc, char** argv)
{
	slog("test\n");

	if(argc >= 1)
	{
		// skip the first program name
		--argc;
		++argv;
	}

	cvar_init();

	bool success = true;

	for(int i = 0; i < argc; ++i)
	{
		if(strcmp(argv[i], "--help") == 0)
		{
			const char* usage_message = "Usage: %s [--options] [+cv_option \"0\"]\n"
										"\t--help\tshow this usage message\n"
										"\t--list-cvars\tlist all cv vars options\n"
										"\tnote that you must put cvars after options\n";
			slogf(usage_message, "prog_name");
			return 0;
		}
		if(strcmp(argv[i], "--list-cvars") == 0)
		{
			cvar_list(false);
			return 0;
		}
		if(strcmp(argv[i], "--list-cvars-debug") == 0)
		{
			cvar_list(true);
			return 0;
		}
		if(argv[i][0] == '+')
		{
			if(!cvar_args(CVAR_T::STARTUP, argc - i, argv + i))
			{
				success = false;
			}
			break;
		}

		serrf("ERROR: unknown argument: %s\n", argv[i]);
		success = false;
	}

	if(success)
	{
		if(!app_init(g_app))
		{
			success = false;
		}
		else
		{
			demo_state demo;
			if(!demo.init())
			{
				success = false;
			}
			else
			{
				bool quit = false;
				while(!quit)
				{
					switch(demo.process())
					{
					case DEMO_RESULT::CONTINUE: break;
					case DEMO_RESULT::EXIT: quit = true; break;
					case DEMO_RESULT::ERROR:
						quit = true;
						success = false;
						break;
					}
				}
			}
			if(!demo.destroy())
			{
				success = false;
			}
		}
		app_destroy(g_app);
	}

	if(!success)
	{
		// TODO: probably should wrap this around a wrapper, and limit the string to a certain width
		// and height.
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", serr_get_error().c_str(), NULL);
	}

	// last chance to catch any serr messages (hopefully there wasn't anything with static
	// destructors....) it's not neccessary to place this EVERYWHERE because serrf could be set to
	// print a stacktrace.
	if(serr_check_error())
	{
		serrf("\nUncaught error before exit\n");
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_WARNING, "Uncaught error", serr_get_error().c_str(), NULL);
	}

	return 0;
}
