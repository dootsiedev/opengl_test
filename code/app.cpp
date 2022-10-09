#include "global_pch.h"
#include "global.h"

#include "app.h"

#include "opengles2/opengl_stuff.h"

App_Info g_app;

REGISTER_CVAR_INT(
	cv_startup_screen_width,
	640,
	"screen width on startup, and when you reset the window size",
	CVAR_T::STARTUP);
REGISTER_CVAR_INT(
	cv_startup_screen_height,
	480,
	"screen height on startup, and when you reset the window size",
	CVAR_T::STARTUP);
REGISTER_CVAR_INT(cv_screen_width, 640, "current screen width", CVAR_T::READONLY);
REGISTER_CVAR_INT(cv_screen_height, 480, "current screen height", CVAR_T::READONLY);

REGISTER_CVAR_INT(
	cv_debug_opengl,
	0,
	"0 = off, 1 = show detailed opengl errors, 2 = stacktrace per call",
	CVAR_T::STARTUP);

static CVAR_T opengl_gamma_correct_type = 
#ifdef __EMSCRIPTEN__
CVAR_T::DISABLED;
#else
CVAR_T::STARTUP;
#endif

static REGISTER_CVAR_INT(
	cv_opengl_gamma_correct, 0, "0 = off, 1 = request gamma correction", opengl_gamma_correct_type);

// this is for GL_FRAMEBUFFER_SRGB_EXT
// I probably shouldn't use it, and opt for doing it in the shader or use SRGB_ALPHA_EXT
static REGISTER_CVAR_INT(cv_has_gl_srgb, -1, "0 = off, 1 = on, -1 = unknown", CVAR_T::READONLY);

REGISTER_CVAR_DOUBLE(cv_scroll_speed, 3, "scroll rate of the mouse wheel", CVAR_T::RUNTIME);

// modifying cv_ui_scale will not look good because it's hard to update everything perfectly.
// most elements can fix their sizes by triggering a resize event
// some ui menus will not be fixed by resize because I cache the size
// not sure if I care enough to actually fix that.
// maybe I could do a trick where I allow the font size to be resized in the options menu,
// but until you restart the font will use cv_ui_scale?
REGISTER_CVAR_DOUBLE(
	cv_ui_scale, 1, "scale the fonts, but the font will look upscaled", CVAR_T::DEFFERRED);

cvar_fullscreen cv_fullscreen;
cvar_vysnc cv_vsync;

// this will change the display resolution on windows 7 or linux (in very annoying effect)
// also some people say that changing the resolution can damage your display
// (but this only happened to certain CRT's with a faulty design)
// on windows 10 SDL_WINDOW_FULLSCREEN is the same as SDL_WINDOW_FULLSCREEN_DESKTOP.
// on emscripten this scales the resolution ONLY when you use soft_fullscreen (alt+enter)
static REGISTER_CVAR_INT(
	cv_stretch_fullscreen,
	0,
	"this tries to stretch the resolution (windows 10 wont work)",
	CVAR_T::RUNTIME);

#ifdef __EMSCRIPTEN__
//  TODO: export the console so the web page has a backup prompt.
//  TODO: some sort of hotkey to restart with startup cvars (or use php url thingy for it?)
//      or should I just have the web page paused on start so you can enter the cvars?
//  TODO: clipboard requires async events and there are portability problems
//      but it's still possible I just need to make keyboard events use the callback
//      (unless I am wrong that I need a short handler to grab the clipboard?)
//      and use callbacks to save and load the clipboard asynchronously.
// this emscripten code is based heavily on test/test_html5_fullscreen.c
const char* emscripten_event_type_to_string(int eventType)
{
	const char* events[] = {
		"(invalid)",
		"(none)",
		"keypress",
		"keydown",
		"keyup",
		"click",
		"mousedown",
		"mouseup",
		"dblclick",
		"mousemove",
		"wheel",
		"resize",
		"scroll",
		"blur",
		"focus",
		"focusin",
		"focusout",
		"deviceorientation",
		"devicemotion",
		"orientationchange",
		"fullscreenchange",
		"pointerlockchange",
		"visibilitychange",
		"touchstart",
		"touchend",
		"touchmove",
		"touchcancel",
		"gamepadconnected",
		"gamepaddisconnected",
		"beforeunload",
		"batterychargingchange",
		"batterylevelchange",
		"webglcontextlost",
		"webglcontextrestored",
		"(invalid)"};
	++eventType;
	if(eventType < 0) eventType = 0;
	if(static_cast<size_t>(eventType) >= sizeof(events) / sizeof(events[0]))
		eventType = sizeof(events) / sizeof(events[0]) - 1;
	return events[eventType];
}
const char* emscripten_result_to_string(EMSCRIPTEN_RESULT result)
{
	if(result == EMSCRIPTEN_RESULT_SUCCESS) return "EMSCRIPTEN_RESULT_SUCCESS";
	if(result == EMSCRIPTEN_RESULT_DEFERRED) return "EMSCRIPTEN_RESULT_DEFERRED";
	if(result == EMSCRIPTEN_RESULT_NOT_SUPPORTED) return "EMSCRIPTEN_RESULT_NOT_SUPPORTED";
	if(result == EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED)
		return "EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED";
	if(result == EMSCRIPTEN_RESULT_INVALID_TARGET) return "EMSCRIPTEN_RESULT_INVALID_TARGET";
	if(result == EMSCRIPTEN_RESULT_UNKNOWN_TARGET) return "EMSCRIPTEN_RESULT_UNKNOWN_TARGET";
	if(result == EMSCRIPTEN_RESULT_INVALID_PARAM) return "EMSCRIPTEN_RESULT_INVALID_PARAM";
	if(result == EMSCRIPTEN_RESULT_FAILED) return "EMSCRIPTEN_RESULT_FAILED";
	if(result == EMSCRIPTEN_RESULT_NO_DATA) return "EMSCRIPTEN_RESULT_NO_DATA";
	return "Unknown EMSCRIPTEN_RESULT!";
}

EM_BOOL on_canvassize_changed(
	int, const void*, void*) //(int eventType, const void* reserved, void* userData)
{
	int w, h;
	EMSCRIPTEN_RESULT em_ret = emscripten_get_canvas_element_size("#canvas", &w, &h);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_get_canvas_element_size",
			emscripten_result_to_string(em_ret));
	}
	SDL_SetWindowSize(g_app.window, w, h);

	// I set the window size twice so that SDL will be forced to trigger a resize event.
	// I also could do nothing, and create a fake resize event.
	if(cv_stretch_fullscreen.data == 1)
	{
		SDL_SetWindowSize(
			g_app.window, cv_startup_screen_width.data, cv_startup_screen_height.data);
	}

	// double cssW, cssH;
	// emscripten_get_element_css_size(0, &cssW, &cssH);
	// slogf("Canvas resized: WebGL RTT size: %dx%d, canvas CSS size: %02gx%02g\n", w, h, cssW,
	// cssH);
	//      SDL_SetWindowSize(g_app.window, w, h);
	// slogf("Canvas resized: WebGL RTT size: %dx%d\n", w, h);
	return 0;
}

#if 0
EM_BOOL fullscreenchange_callback(int eventType, const EmscriptenFullscreenChangeEvent *e, void *userData)
{
  slogf("%s, isFullscreen: %d, fullscreenEnabled: %d, fs element nodeName: \"%s\", fs element id: \"%s\". New size: %dx%d pixels. Screen size: %dx%d pixels.\n",
    emscripten_event_type_to_string(eventType), e->isFullscreen, e->fullscreenEnabled, e->nodeName, e->id, e->elementWidth, e->elementHeight, e->screenWidth, e->screenHeight);
    
    // I need to use emscripten_request_fullscreen_strategy to trigger real_fullscreen with proper scaling.
    // but from looking at the html JS, it uses "Module"???
#if 0
    if(e->isFullscreen == 1)
    {
        EmscriptenFullscreenStrategy s;
        memset(&s, 0, sizeof(s));
        s.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
        s.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
        s.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
        s.canvasResizedCallback = on_canvassize_changed;
        EMSCRIPTEN_RESULT em_ret = emscripten_enter_soft_fullscreen("#canvas", &s);
        if (em_ret != EMSCRIPTEN_RESULT_SUCCESS)
        {
            slogf("%s returned %s.\n", "emscripten_enter_soft_fullscreen", emscripten_result_to_string(em_ret));
        }
    }
    else
    {
        emscripten_exit_soft_fullscreen();
        //SDL_SetWindowSize(g_app.window, original_w, original_h);
    }
#endif
  return 0;
}
#endif

// trigger this with an html button because html will only trigger fullscreen if you do it in a
// correct handler.
extern "C" {
extern void enter_fullscreen()
{
#if 0
        EmscriptenFullscreenStrategy s;
        memset(&s, 0, sizeof(s));
        s.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
        s.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
        s.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
        s.canvasResizedCallback = 0; // on_canvassize_changed;
        // deferred means to my understanding that if this fails,
        // the next event will fullscreen (so the next click would trigger fullscreen)
        EMSCRIPTEN_RESULT em_ret = emscripten_request_fullscreen_strategy("#canvas", 1, &s);
        if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
        {
            slogf(
                "%s returned %s.\n",
                "emscripten_request_fullscreen_strategy",
                emscripten_result_to_string(em_ret));
        }
#endif
	if(SDL_SetWindowFullscreen(
		   g_app.window,
		   // I don't use SDL_WINDOW_FULLSCREEN, because it breaks everything
		   SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
	{
		slogf("SDL_SetWindowFullscreen Error: %s", SDL_GetError());
	}
}
}

static const char* fullscreen_button_string = "#fullscreen_button";
static int on_fullscreen_button_click(
	int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
	(void)userData; // unused
	(void)mouseEvent; // unused
	if(eventType == EMSCRIPTEN_EVENT_CLICK)
	{
		enter_fullscreen();
		return 1;
	}
	return 0;
}

// static int original_w = 0;
// static int original_h = 0;

#endif

bool app_init(App_Info& app)
{
	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		serrf("SDL_Init Error: %s", SDL_GetError());
		return false;
	}

#ifdef __EMSCRIPTEN__
#if 0
    EMSCRIPTEN_RESULT em_ret = emscripten_set_fullscreenchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, fullscreenchange_callback);
    if (em_ret != EMSCRIPTEN_RESULT_SUCCESS)
    {
        slogf("%s returned %s.\n", "emscripten_set_fullscreenchange_callback", emscripten_result_to_string(em_ret));
    }
#endif
	EMSCRIPTEN_RESULT em_ret = emscripten_set_click_callback(
		fullscreen_button_string, NULL, 0, on_fullscreen_button_click);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s returned %s.\n",
			"emscripten_set_click_callback",
			emscripten_result_to_string(em_ret));
	}

	if(cv_fullscreen.data == 1)
	{
		slogf("warning: emscripten should not start with fullscreen.\n");
	}

	// original_w = cv_screen_width.data;
	// original_h = cv_screen_height.data;
#endif

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
	// SDL_CHECK(SDL_SetHint("SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH", "1"));
#ifdef HAS_IME_TEXTEDIT_EXT
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
	if(cv_opengl_gamma_correct.data == 1)
	{
		SDL_CHECK(SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1));
	}
#undef SDL_CHECK

	Uint32 fullscreen_mode = cv_fullscreen.data == 1
								 ? (cv_stretch_fullscreen.data == 1 ? SDL_WINDOW_FULLSCREEN
																	: SDL_WINDOW_FULLSCREEN_DESKTOP)
								 : 0;

	app.window = SDL_CreateWindow(
		"A Window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		cv_startup_screen_width.data,
		cv_startup_screen_height.data,
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

	cv_has_gl_srgb.data =
		(SDL_GL_ExtensionSupported("GL_EXT_sRGB_write_control") == SDL_FALSE) ? 0 : 1;

	if(cv_opengl_gamma_correct.data == 1)
	{
		if(cv_has_gl_srgb.data != 1)
		{
			slog("warning cv_opengl_gamma_correct: GL_EXT_sRGB_write_control unsupported\n");
		}
		else
		{
			if((gl_context_flags & SDL_GL_FRAMEBUFFER_SRGB_CAPABLE) == 0)
			{
				slog("warning: SDL_GL_FRAMEBUFFER_SRGB_CAPABLE failed to set\n");
			}
			ctx.glEnable(GL_FRAMEBUFFER_SRGB_EXT);
		}
	}

	// note I already check cv_has_GL_KHR_debug inside LoadGLContext
	// maybe I should bring that here?
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

	// I don't know why by emscripten gets bugged out by SDL_GL_GetDrawableSize.
	// int w;
	// int h;
	// SDL_GL_GetDrawableSize(app.window, &w,&h);
	// cv_screen_width.data = w;
	// cv_screen_height.data = h;
	cv_screen_width.data = cv_startup_screen_width.data;
	cv_screen_height.data = cv_startup_screen_height.data;
	ctx.glViewport(0, 0, cv_screen_width.data, cv_screen_height.data);

	return true;
}

bool app_destroy(App_Info& app)
{
	bool success = true;
	if(app.gl_context != NULL)
	{
		if(SDL_GL_GetCurrentContext() != app.gl_context)
		{
			serr("gl context was not bound as the current context\n");
			success = false;
		}
		SDL_GL_DeleteContext(app.gl_context);
		app.gl_context = NULL;
		memset(&ctx, 0, sizeof(ctx));
	}

	if(app.window != NULL)
	{
		SDL_ClearError();
		SDL_DestroyWindow(app.window);
		const char* error = SDL_GetError();
		if(error[0] != '\0')
		{
			serrf("error SDL_DestroyWindow: %s\n", SDL_GetError());
			success = false;
		}
		app.window = NULL;
	}

#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_RESULT em_ret =
		emscripten_set_click_callback(fullscreen_button_string, (void*)0, 0, NULL);
	if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
	{
		slogf(
			"%s(\"%s\") returned %s.\n",
			"emscripten_set_click_callback",
			fullscreen_button_string,
			emscripten_result_to_string(em_ret));
	}
#endif

	SDL_Quit();
	return success;
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
#ifdef __EMSCRIPTEN__
		// this is a hack to force soft fullscreen to play nicely with the fullscreen button on the
		// emscripten demo page
		EmscriptenFullscreenChangeEvent fsce;
		EMSCRIPTEN_RESULT em_ret = emscripten_get_fullscreen_status(&fsce);
		if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
		{
			slogf(
				"%s returned %s.\n",
				"emscripten_get_fullscreen_status",
				emscripten_result_to_string(em_ret));
		}
		else
		{
			if(fsce.isFullscreen)
			{
				slogf("info: exit fullscreen\n");
				data = 0;
				em_ret = emscripten_exit_fullscreen();
				if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
				{
					slogf(
						"%s returned %s.\n",
						"emscripten_exit_fullscreen",
						emscripten_result_to_string(em_ret));
				}
				// there is a weird glitch I forgot how to reproduce,
				// it has to do with mixing soft and full fullscreen,
				// but essentially emscripten forgets the original size of the window.
				// UPDATE: But it turns out that this didn't even fix the problem...
				// The problem comes from a failed fullscreen request due to long running handler.
				// SDL_SetWindowSize(g_app.window, original_w, original_h);
				return ret;
			}
		}
		// use soft fullscreen
		if(data == 1)
		{
			EmscriptenFullscreenStrategy s;
			memset(&s, 0, sizeof(s));
			s.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_DEFAULT;
			s.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_STDDEF;
			s.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
			s.canvasResizedCallback = on_canvassize_changed;
			em_ret = emscripten_enter_soft_fullscreen("#canvas", &s);
			if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
			{
				slogf(
					"%s returned %s.\n",
					"emscripten_enter_soft_fullscreen",
					emscripten_result_to_string(em_ret));
			}

			if(cv_stretch_fullscreen.data == 1)
			{
				SDL_SetWindowSize(
					g_app.window, cv_startup_screen_width.data, cv_startup_screen_height.data);
			}
			else
			{
				int w, h;
				em_ret = emscripten_get_canvas_element_size("#canvas", &w, &h);
				if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
				{
					slogf(
						"%s returned %s.\n",
						"emscripten_get_canvas_element_size",
						emscripten_result_to_string(em_ret));
				}
				SDL_SetWindowSize(g_app.window, w, h);
			}
		}
		else
		{
			emscripten_exit_soft_fullscreen();
			int w, h;
			em_ret = emscripten_get_canvas_element_size("#canvas", &w, &h);
			if(em_ret != EMSCRIPTEN_RESULT_SUCCESS)
			{
				slogf(
					"%s returned %s.\n",
					"emscripten_get_canvas_element_size",
					emscripten_result_to_string(em_ret));
			}

			SDL_SetWindowSize(g_app.window, w, h);
		}
#else
		Uint32 fullscreen_mode =
			data == 1 ? (cv_stretch_fullscreen.data == 1 ? SDL_WINDOW_FULLSCREEN
														 : SDL_WINDOW_FULLSCREEN_DESKTOP)
					  : 0;
		if(SDL_SetWindowFullscreen(g_app.window, fullscreen_mode) < 0)
		{
			slogf("info: SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
		}
#endif
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
