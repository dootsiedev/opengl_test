#include "global_pch.h"
#include "global.h"

#include "debug_tools.h"

#include "cvar.h"

#include <cinttypes>
#include <cstdio>

// because I don't include global.h
#include <cassert>
#ifndef ASSERT
#define ASSERT assert
#endif

#include <memory>

// platform specific debug breakpoint headers.
#if defined(_WIN32)
#include <intrin.h> // for __debugbreak
#include <debugapi.h> // for IsDebuggerPresent
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
// assume linux and has SIGTRAP
#include <signal.h>
#endif

static int has_libbacktrace
#if defined(USE_LIBBACKTRACE)
	= 1;
#else
	= 0;
#endif
static REGISTER_CVAR_INT(
	cv_has_libbacktrace, has_libbacktrace, "0 = not found, 1 = found", CVAR_T::READONLY);

static int enable_bt_trap
#if defined(_WIN32) && !defined(USE_LIBBACKTRACE)
	// Libbacktrace isn't supported on Windows msvc.
	// I know I can get msvc stacktraces, but sharing symbols a pain in the ass.
	// So instead I use core dumps for any user crash reports.
	// core dumps are inconvenient, but if a user has a problem they can reproduce,
	// they can use the debug binary and give a very informational (100gig...) report.
	// OR I could use msys2 which supports libbacktrace (but no core dumps...)
	= 2;
#elif defined(USE_LIBBACKTRACE) || defined(NDEBUG) || defined(__EMSCRIPTEN__)
	// NDEBUG means optimizations, and trapping for a crappy backtrace is annoying.
	// if you have libbacktrace, the trap would (usually) prevent the stacktrace from being shown.
	= 0;
#else
	// sometimes a trap is better than nothing,
	// and if you have debug information, you can inspect many variables.
	// but the trap is annoying (especially from "cv_serr_bt")
	// because without a debugger, it just causes your application to exit!
	= 1;
#endif

static CVAR_T cv_bt_trap_run
#if defined(__EMSCRIPTEN__)
	// the browser doesn't have gdb or lldb that works...
	// no point in trapping....
	= CVAR_T::DISABLED;
#else
	= CVAR_T::RUNTIME;
#endif
static REGISTER_CVAR_INT(
	cv_bt_trap,
	enable_bt_trap,
	"replace all stacktraces with a debug trap, 0 (off), 1 (on), 2 (windows only: only trap inside a debugger)",
	cv_bt_trap_run);

static CVAR_T has_libbacktrace_run
#if defined(USE_LIBBACKTRACE)
	= CVAR_T::RUNTIME;
#else
	= CVAR_T::DISABLED;
#endif
static REGISTER_CVAR_INT(
	cv_bt_demangle, 1, "stacktrace pretty function names, 0 (off), 1 (on)", has_libbacktrace_run);
static REGISTER_CVAR_INT(
	cv_bt_full_paths, 0, "stacktrace full file paths, 0 (off), 1 (on)", has_libbacktrace_run);

#if !defined(__EMSCRIPTEN__)

// I haven't tested it, but I think msys could work with libbacktrace.
#ifdef USE_LIBBACKTRACE
#include <backtrace.h>

#if defined(__linux__)
#include <dlfcn.h> // for dladdr
#endif

#ifdef __GNUG__
#include <cxxabi.h>
#endif

struct bt_payload
{
	debug_stacktrace_callback call = NULL;
	int idx = 0;
	void* ud = NULL;
	// nested syminfo call...
	backtrace_state* state;
	// I don't want to get the module again when I use syminfo.
	const char* syminfo_module = NULL;
};

static void bt_error_callback(void* vdata, const char* msg, int errnum)
{
	(void)errnum;
	ASSERT(vdata != NULL);
	bt_payload* payload = static_cast<bt_payload*>(vdata);
	ASSERT(payload->call != NULL);

	if(payload == NULL || payload->call == NULL)
	{
		fprintf(stderr, "libbacktrace error: %s\n", (msg != NULL ? msg : "no error?"));
		return;
	}

	payload->call(NULL, (msg != NULL ? msg : "no error?"), payload->ud);
}

static void bt_syminfo_callback(
	void* vdata, uintptr_t pc, const char* function, uintptr_t symval, uintptr_t symsize)
{
	bt_payload* payload = static_cast<bt_payload*>(vdata);
	(void)symval;
	(void)symsize;
#ifdef __GNUG__
	auto free_del = [](void* ptr) { free(ptr); };
	std::unique_ptr<char, decltype(free_del)> demangler{NULL, free_del};
	if(cv_bt_demangle.data != 0 && function != NULL)
	{
		int status = 0;
		demangler.reset(abi::__cxa_demangle(function, NULL, NULL, &status));
		if(status == 0)
		{
			function = demangler.get();
		}
	}
#endif
	debug_stacktrace_info info{payload->idx, pc, payload->syminfo_module, function, NULL, 0};

	payload->call(&info, NULL, payload->ud);
}

static int bt_full_callback(
	void* vdata, uintptr_t pc, const char* filename, int lineno, const char* function)
{
	bt_payload* payload = static_cast<bt_payload*>(vdata);

	// don't know why this is always at the bottom of the stack in libbacktrace
	if(pc == static_cast<uintptr_t>(-1))
	{
		return 0;
	}

	++payload->idx;

	const char* module_name = NULL;

	// on win32 this could be replicated through GetModuleHandleEx + GetModuleFileNameW
	// but I don't think you can get the function name without symbols...
#if defined(__linux__)

	// gotta memcpy because it's possible uintptr_t and void* have incompatible alignment
	// clang-tidy will nag me if I cast it...
	void* addr;
	memcpy(&addr, &pc, sizeof(void*));

	Dl_info dinfo;
	if(dladdr(addr, &dinfo) != 0)
	{
		module_name = dinfo.dli_fname;
		if(cv_bt_full_paths.data == 0)
		{
			const char* temp_module_name = strrchr(dinfo.dli_fname, '/');
			if(temp_module_name != NULL)
			{
				module_name = temp_module_name;
			}
		}
	}

	// a fallback, this only works because the functions are public
	// this won't work if you use -fvisibility=hidden, or inline optimizations.
	if(function == NULL)
	{
		function = dinfo.dli_sname;
	}
	if(function == NULL)
	{
		// backtrace_syminfo requires the symbol table but does not require the debug info
		payload->syminfo_module = module_name;
		// I don't care about the error.
		if(backtrace_syminfo(payload->state, pc, bt_syminfo_callback, bt_error_callback, vdata) ==
		   1)
		{
			return 0;
		}
	}
#endif

#ifdef __GNUG__
	auto free_del = [](void* ptr) { free(ptr); };
	std::unique_ptr<char, decltype(free_del)> demangler{NULL, free_del};
	if(cv_bt_demangle.data != 0 && function != NULL)
	{
		int status = 0;
		demangler.reset(abi::__cxa_demangle(function, NULL, NULL, &status));
		if(status == 0)
		{
			function = demangler.get();
		}
	}
#endif

	if(filename != NULL)
	{
		if(cv_bt_full_paths.data == 0)
		{
			const char* temp_filename = strrchr(filename, '/');
			if(temp_filename != NULL)
			{
				filename = temp_filename;
			}
		}
	}

	debug_stacktrace_info info{payload->idx, pc, module_name, function, filename, lineno};

	return payload->call(&info, NULL, payload->ud);
}

// wrapper because static initialization of a constructor is thread safe.
struct bt_state_wrapper
{
	backtrace_state* state;
	explicit bt_state_wrapper(bt_payload& info)
	: state(backtrace_create_state(NULL, 1, bt_error_callback, &info))
	{
	}
};

__attribute__((noinline)) bool
	debug_raw_stacktrace(debug_stacktrace_callback callback, void* ud, int skip)
{
#if defined(_WIN32)
	if(cv_bt_trap.data == 1 || (cv_bt_trap.data == 2 && IsDebuggerPresent()))
	{
		__debugbreak();
	}
#else
	if(cv_bt_trap.data == 1)
	{
		raise(SIGTRAP);
	}
#endif

	bt_payload info;
	info.call = callback;
	info.ud = ud;
	info.idx = 0;
	info.syminfo_module = NULL;

	static bt_state_wrapper state(info);
	info.state = state.state;
	if(state.state == NULL)
	{
		return false;
	}
#if 0
	TIMER_U tick1;
	TIMER_U tick2;
	tick1 = timer_now();
#endif
	// resolving the debug info is very slow, but you could offload the overhead by using
	// backtrace_simple and in another thread you can resolving the debug info from backtrace_pcinfo
	int ret = backtrace_full(state.state, skip + 1, bt_full_callback, bt_error_callback, &info);

#if 0
	tick2 = timer_now();
	slogf("bt time = %f\n", timer_delta_ms(tick1, tick2));
#endif
	return ret == 0;
}

// format: MODULE ! FUNCTION [FILE @ LINE] or MODULE ! PTR
int raw_string_callback(debug_stacktrace_info* data, const char* error, void* ud)
{
	ASSERT(ud != NULL);

	// this might be slower than std::ostringstream
	// but it's faster if you reuse the same std::string.
	std::string* output = reinterpret_cast<std::string*>(ud);

	// is this an error frame?
	if(data == NULL)
	{
		ASSERT(error != NULL);
		*output += __func__;
		*output += ": ";
		*output += error;
		return 0;
	}

	char buffer[500];

	if(data->module != NULL)
	{
		int ret = snprintf(buffer, sizeof(buffer), "%s", data->module);
		if(ret < 0)
		{
			goto err;
		}
		output->append(buffer, std::min<size_t>(ret, sizeof(buffer) - 1));
	}

	if(data->function != NULL)
	{
		int ret = snprintf(buffer, sizeof(buffer), " ! in %s", data->function);
		if(ret < 0)
		{
			goto err;
		}
		output->append(buffer, std::min<size_t>(ret, sizeof(buffer) - 1));
	}
	else
	{
		int ret = snprintf(buffer, sizeof(buffer), " ! %" PRIxPTR, data->addr);
		if(ret < 0)
		{
			goto err;
		}
		output->append(buffer, std::min<size_t>(ret, sizeof(buffer) - 1));
	}

	if(data->file != NULL)
	{
		int ret = snprintf(buffer, sizeof(buffer), " [%s @ %d]", data->file, data->line);
		if(ret < 0)
		{
			goto err;
		}
		output->append(buffer, std::min<size_t>(ret, sizeof(buffer) - 1));
	}

	output->push_back('\n');

	return 0;
err:
	*output += __func__;
	*output += " snprintf error: ";
	*output += strerror(errno);
	*output += '\n';
	return -1;
}
#else

int raw_string_callback(debug_stacktrace_info*, const char*, void*)
{
	return 0;
}

bool debug_raw_stacktrace(debug_stacktrace_callback, void*, int)
{
#if defined(_WIN32)
	if(cv_bt_trap.data == 1 || (cv_bt_trap.data == 2 && IsDebuggerPresent()))
	{
		__debugbreak();
	}
#else
	if(cv_bt_trap.data == 1)
	{
		raise(SIGTRAP);
	}
#endif
	return false;
}

#endif

#endif
