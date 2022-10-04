#include "global_pch.h"
#include "global.h"
#include "app.h"
#include "demo.h"
#include <SDL2/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// TODO: use emscripten_get_now, maybe it might fix the very bad timers

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
		slogf("info: cvar file found: %s\n", path);
		RWops_Stdio fp_raii(fp, path);
		if(!cvar_file(CVAR_T::STARTUP, &fp_raii))
		{
			success = false;
		}
		if(!fp_raii.close())
		{
			success = false;
		}
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
			demo_state demo;
			if(!demo.init())
			{
				success = false;
			}
			else
			{
#ifdef __EMSCRIPTEN__
                //TODO: this should be changed to use not use simulate_infinite_loop
                // because I think some profiling feature wont work or something.
				struct shitty_payload
				{
					demo_state* p_demo;
					bool* p_success;
				} pay{&demo, &success};
				auto loop = [](void* ud) {
					shitty_payload* p_pay = static_cast<shitty_payload*>(ud);
					switch(p_pay->p_demo->process())
					{
					case DEMO_RESULT::CONTINUE: break;
					case DEMO_RESULT::EXIT: emscripten_cancel_main_loop(); break;
					case DEMO_RESULT::ERROR:
						emscripten_cancel_main_loop();
						*p_pay->p_success = false;
						break;
					}
				};
				emscripten_set_main_loop_arg(loop, &pay, 0, 1);
#else
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
#endif
			}
			if(!demo.destroy())
			{
				success = false;
			}
		}
		if(!app_destroy(g_app))
		{
			success = false;
		}
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
