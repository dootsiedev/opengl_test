#pragma once

#include <string>

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
// skip is ignored...
inline int debug_str_stacktrace(std::string* out, int)
{
    char buffer[10000];
    // no error code, includes size of null terminator so I use -1.
    int ret = emscripten_get_callstack(0, buffer, sizeof(buffer));
    out->append(buffer, ret - 1);
    return 0;
}
#else

struct debug_stacktrace_info
{
	int index;
	uintptr_t addr;
	const char* module;
	const char* function;
	const char* file;
	int line;
};

// return zero for success
typedef int (*debug_stacktrace_callback)(debug_stacktrace_info* data, const char* error, void* ud);

// false means no stacktrace, no serr message.
__attribute__((noinline)) bool
	debug_raw_stacktrace(debug_stacktrace_callback callback, void* ud, int skip = 0);

// format: MODULE ! FUNCTION [FILE @ LINE] or MODULE ! PTR
int raw_string_callback(debug_stacktrace_info* data, const char* error, void* ud);
#define debug_str_stacktrace(out, skip) debug_raw_stacktrace(raw_string_callback, out, skip)
#endif