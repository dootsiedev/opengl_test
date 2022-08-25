#pragma once

#include <string>

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
bool debug_raw_stacktrace(debug_stacktrace_callback callback, void* ud, int skip = 0);

// format: MODULE ! FUNCTION [FILE @ LINE] or MODULE ! PTR
int raw_string_callback(debug_stacktrace_info* data, const char* error, void* ud);
#define debug_str_stacktrace(out, skip) debug_raw_stacktrace(raw_string_callback, out, skip)