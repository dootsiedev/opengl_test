#include "global_pch.h"
#include "global.h"
#include "app.h"
#include "demo.h"
#include <SDL2/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static std::unique_ptr<demo_state> p_demo;
void emscripten_loop()
{
	bool hard_exit = false;
	bool success_loop = true;
	ASSERT(p_demo);
	switch(p_demo->process())
	{
	case DEMO_RESULT::CONTINUE: break;
	case DEMO_RESULT::SOFT_REBOOT:
		if(!p_demo->destroy())
		{
			success_loop = false;
			p_demo.reset();
		}
		else
		{
			p_demo = std::make_unique<demo_state>();
			if(!p_demo->init())
			{
				success_loop = false;
			}
		}
		break;
	case DEMO_RESULT::EXIT: hard_exit = true; break;
	case DEMO_RESULT::ERROR: success_loop = false; break;
	}

	if(hard_exit || !success_loop)
	{
		if(p_demo)
		{
			if(!p_demo->destroy())
			{
				success_loop = false;
			}
			p_demo.reset();
		}
		if(!app_destroy(g_app))
		{
			success_loop = false;
		}
		emscripten_cancel_main_loop();
	}

	if(!success_loop)
	{
		// TODO: probably should wrap this around a wrapper, and limit the string to a certain width
		// and height.
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", serr_get_error().c_str(), NULL);
	}

    // check for leaked serr errors
	if(serr_check_error())
	{
		// you probably want to use cv_serr_bt to find the location of the leak.
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_WARNING, "Uncaught error", serr_get_error().c_str(), NULL);
	}
};
#endif

int main(int argc, char** argv)
{
	slog("test\n");

	const char* prog_name = NULL;
	if(argc >= 1)
	{
		// skip the first program name
		prog_name = argv[0];
		--argc;
		++argv;
	}

	cvar_init();

	bool success = true;

	const char* path = "cvar.cfg";
	FILE* fp = fopen(path, "rb");
	if(fp == NULL)
	{
		// not existing is not an error (maybe make an info log?)
		// ENOENT = No such file or directory
		if(errno != ENOENT)
		{
			serrf("Failed to open: `%s`, reason: %s\n", path, strerror(errno));
			success = false;
		}
	}
	else
	{
		slogf("info: found cvar file: %s\n", path);
		RWops_Stdio fp_raii(fp, path);
		if(!cvar_file(CVAR_T::STARTUP, &fp_raii))
		{
			success = false;
		}
		if(!fp_raii.close())
		{
			success = false;
		}
		slogf("info: done reading cvar file.\n");
	}

	// load cvar arguments after I load the cvar file
	// I probably shouldn't so the --help output could be cleaner,
	// but it doesn't matter much.
	if(success)
	{
		for(int i = 0; i < argc; ++i)
		{
			if(strcmp(argv[i], "--help") == 0)
			{
				const char* usage_message = "Usage: %s [--options] [+cv_option \"0\"]\n"
											"\t--help\tshow this usage message\n"
											"\t--list-cvars\tlist all cv vars options\n"
											"\tnote that you must put cvars after options\n";
				slogf(usage_message, (prog_name != NULL ? prog_name : "prog_name"));
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
				int ret = cvar_arg(CVAR_T::STARTUP, argc - i, argv + i);
				if(ret == -1)
				{
					success = false;
				}
				argc -= ret;
				argv += ret;
				continue;
			}

			serrf("ERROR: unknown argument: %s\n", argv[i]);
			success = false;
		}
	}

	if(success)
	{
		if(!app_init(g_app))
		{
			success = false;
		}
		else
		{
#ifdef __EMSCRIPTEN__

			p_demo = std::make_unique<demo_state>();
			if(!p_demo->init())
			{
				success = false;
			}
			else
			{
				// this will fall through
				emscripten_set_main_loop(emscripten_loop, 0, 0);
			}
#else
			bool reboot;
			do
			{
				reboot = false;

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
						case DEMO_RESULT::SOFT_REBOOT:
							reboot = true;
							quit = true;
							break;
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
					// I want to present a dialog window if this was rebooting
					break;
				}
			} while(reboot);
#endif
		}
#ifndef __EMSCRIPTEN__
		if(!app_destroy(g_app))
		{
			success = false;
		}
#endif
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
		// you probably want to use cv_serr_bt to find the location of the leak.
		serrf("\nUncaught error before exit\n");
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_WARNING, "Uncaught error", serr_get_error().c_str(), NULL);
	}

	return 0;
}
