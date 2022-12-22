#pragma once

#include "global.h"

#if defined(USE_LIBBACKTRACE) || (!defined(NDEBUG) && defined(__EMSCRIPTEN__))
#define HAS_STACKTRACE_PROBABLY
#endif

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
// skip is ignored...
inline int debug_str_stacktrace(std::string* out, int)
{
	// the problem with this is that there is no way to "skip" calls.
	char buffer[10000];
	// no error code, includes size of null terminator.
	// there is no way to detect if truncation occurred.
    int flags = 0; 
    //#ifdef HAS_STACKTRACE_PROBABLY
    flags |= EM_LOG_C_STACK;
    //#endif
	int ret = emscripten_get_callstack(flags, buffer, sizeof(buffer));
	out->append(buffer, ret - 1);
    out += '\n';
	return 0;
}
#else

// return false means no stacktrace, no serr message.
// format: MODULE ! FUNCTION [FILE @ LINE] or MODULE ! PTR
bool debug_str_stacktrace(std::string* out, int skip);
#endif