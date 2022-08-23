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

#ifndef _WIN32
#include <signal.h>
#else
#include <intrin.h> // for __debugbreak
#endif

// sometimes a trap is better than a stacktrace,
// especially when you don't even have a stacktrace.
// NDEBUG disables the trap because there isn't good debug info.
// Note I disable NDEBUG on relwithdebinfo in cmake
// because checking asserts and trapping on release is useful.
static int default_cv_stacktrace_trap
#if defined(NDEBUG) || defined(USE_LIBBACKTRACE)
	= 0;
#else
	= 1;
#endif
static REGISTER_CVAR_INT(
	cv_stacktrace_trap, default_cv_stacktrace_trap, "0 (off), 1 (on)", CVAR_DEFAULT);

static int has_libbacktrace
#if defined(USE_LIBBACKTRACE)
	= CVAR_DEFAULT;
#else
	= CVAR_DISABLED;
#endif
static REGISTER_CVAR_INT(cv_stacktrace_demangle, 1, "0 (off), 1 (on)", has_libbacktrace);
static REGISTER_CVAR_INT(cv_stacktrace_full_paths, 0, "0 (off), 1 (on)", has_libbacktrace);

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
#if defined(__linux__)

	// gotta memcpy because it's possible uintptr_t and void* have incompatible alignment
	// clang-tidy will nag me if I cast it...
	void* addr;
	memcpy(&addr, &pc, sizeof(void*));

	Dl_info dinfo;
	if(dladdr(addr, &dinfo) != 0)
	{
		module_name = dinfo.dli_fname;
		if(cv_stacktrace_full_paths.data == 0)
		{
			const char* temp_module_name = strrchr(dinfo.dli_fname, '/');
			if(temp_module_name != NULL)
			{
				module_name = temp_module_name + 1;
			}
		}
	}

	// a fallback, this only works because the functions are public
	// this won't work if you use -fvisibility=hidden, or inline optimizations.
	if(function == NULL)
	{
		function = dinfo.dli_sname;
	}
#endif

#ifdef __GNUG__
	std::unique_ptr<char, void (*)(void*)> demangler{NULL, NULL};
	if(cv_stacktrace_demangle.data != 0 && function != NULL)
	{
		int status = 0;
		demangler = std::unique_ptr<char, void (*)(void*)>{
			abi::__cxa_demangle(function, NULL, NULL, &status), std::free};

		if(status == 0)
		{
			function = demangler.get();
		}
	}
#endif

	if(filename != NULL)
	{
		if(cv_stacktrace_full_paths.data == 0)
		{
			const char* temp_filename = strrchr(filename, '/');
			if(temp_filename != NULL)
			{
				filename = temp_filename + 1;
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
	{
		state = backtrace_create_state(NULL, 1, bt_error_callback, &info);
	}
};

bool debug_raw_stacktrace(debug_stacktrace_callback callback, void* ud, int skip)
{
	if(cv_stacktrace_trap.data == 1)
	{
#ifndef _WIN32
		raise(SIGTRAP);
#else
		__debugbreak();
#endif
		return false;
	}

	bt_payload info;
	info.call = callback;
	info.ud = ud;
	info.idx = 0;

	static bt_state_wrapper state(info);
	if(state.state == NULL)
	{
		return false;
	}

	return backtrace_full(state.state, skip + 1, bt_full_callback, bt_error_callback, &info) == 0;
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

bool debug_raw_stacktrace(debug_stacktrace_callback, void*, int)
{
	if(cv_stacktrace_trap.data == 1)
	{
		// I know I could easily make a backtrace without libbacktrace,
		// but I value the source file and line to the point I would rather trap.
#ifndef _WIN32
		raise(SIGTRAP);
#else
		__debugbreak();
#endif
	}
	return false;
}

int raw_string_callback(debug_stacktrace_info*, const char*, void*)
{
	return 0;
}
#endif
