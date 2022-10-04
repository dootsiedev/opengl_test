#pragma once

#include <cassert>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cerrno>
#include <cstring>

#include <SDL2/SDL.h>

#include <string>
#include <memory>

// assert is bad, but sometimes an explosion is the only choice, eg: PTR IS NULL
#define ASSERT assert

// this is a vague signal to say "THIS FUNCTION PRINTS TO SERR, CAPTURE IT!"
// if a function return is not for serr, just use [[nodiscard]].
#define NDSERR [[nodiscard]]

// CHECK is a soft assert, where the error is probably the programmers fault so the message is not
// user friendly, but unlike ASSERT the program can still continue. returns the value of the
// condition, works the same way as assert, and prints a stack trace. WARNING: check prints into
// serr, which should be handled by serr_get_error.
#define CHECK(expr) implement_CHECK((expr), #expr, __FILE__, __LINE__)
bool implement_CHECK(bool cond, const char* expr, const char* file, int line);

// msvc doesn't support __attribute__, unless it is clang-cl.
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x)
#endif

#ifdef _WIN32
// get message from GetLastError()
std::string WIN_GetFormattedGLE();
// convenience conversion to/from win32 specific functions since only "wide" is unicode.
std::string WIN_WideToUTF8(const wchar_t* buffer, int size);
std::wstring WIN_UTF8ToWide(const char* buffer, int size);
#endif

// for SDL_TEXTEDITING_EXT which is better than SDL_TEXTEDITING
// this is also might be used for SDL_IsTextInputShown 
#ifndef HAS_IME_TEXTEDIT_EXT
#if SDL_VERSION_ATLEAST(2, 23, 0)
#define HAS_IME_TEXTEDIT_EXT
#endif
#endif

// premultiply the alpha using uint8_t {r*a,g*a,b*a,a}
#define RGBA8_PREMULT(r, g, b, a)                                                             \
	{                                                                                         \
		static_cast<uint8_t>(                                                                 \
			((static_cast<float>(r) / 255.f) * (static_cast<float>(a) / 255.f)) * 255.f),     \
			static_cast<uint8_t>(                                                             \
				((static_cast<float>(g) / 255.f) * (static_cast<float>(a) / 255.f)) * 255.f), \
			static_cast<uint8_t>(                                                             \
				((static_cast<float>(b) / 255.f) * (static_cast<float>(a) / 255.f)) * 255.f), \
			a                                                                                 \
	}

struct nocopy
{
	nocopy() = default;
	nocopy(const nocopy&) = delete;
	nocopy& operator=(const nocopy&) = delete;
};

// preserve the original value of errno.
struct pop_errno_t
{
	int old_errno;
	pop_errno_t()
	: old_errno(errno)
	{
		errno = 0;
	}
	~pop_errno_t()
	{
		errno = old_errno;
	}
};

// I need this because it is more robust to not use a template<> for the resolution,
// but I have no idea how to remove the template for chrono.
#define timer_delta_ms(start, end) timer_delta<1000>(start, end)

typedef double TIMER_RESULT;

// use chrono or SDL's QPC
// but QPC's shouldn't be used if timers from other threads are compared.
// you can also try clang's __builtin_readcyclecounter
// if you are measuring really tiny calculations.
#ifdef __EMSCRIPTEN__

#include <chrono>

#define TIMER_NULL std::chrono::steady_clock::time_point{}

typedef std::chrono::steady_clock::time_point TIMER_U;

inline TIMER_U timer_now()
{
	return std::chrono::steady_clock::now();
}

// resolution is based on how much to divide 1 seconds
// 1 = print in seconds, 1000 = print in milliseconds
template<intmax_t resolution>
TIMER_RESULT timer_delta(TIMER_U start, TIMER_U end)
{
	return std::chrono::duration_cast<
			   std::chrono::duration<TIMER_RESULT, std::ratio<1, resolution>>>(end - start)
		.count();
}

#else


typedef Uint64 TIMER_U;

#define TIMER_NULL 0

inline TIMER_U timer_now()
{
	return SDL_GetPerformanceCounter();
}

// resolution is based on how much to divide 1 seconds
// 1 = print in seconds, 1000 = print in milliseconds
template<Uint64 resolution>
inline TIMER_RESULT timer_delta(TIMER_U start, TIMER_U end)
{
	Uint64 frequency = SDL_GetPerformanceFrequency();
	return static_cast<TIMER_RESULT>((end - start) * resolution) /
		   static_cast<TIMER_RESULT>(frequency);
}
#endif

// the main purpose of this logging system is
// to redirect the logs into both stdout and a log file,
// and for being a thread safe logging system,
// but thread safety is only per call, you must always include a newline.

// serr will print into stdout and the log file like slog,
// but the serr message will be buffered into serr_get_error.
// use serr if there is a fatal error that stops everything.
// mainly for initialization and for a GUI message to pop up.
// serr is not useful for a terminal.

// I don't use C++ streams because thread safety is overcomplicated
//(and printf is technically thread safe on common OS's, and if not I can easily make a mutex).
// libfmt is better than printf, but also really weird in my opinion,
// you don't need to think too hard about printf.

// this returns all previous errors into one message, and clears the buffer.
// but ideally there should only be one error at a time.
std::string serr_get_error();

// this will just check if anything was printed to serr.
bool serr_check_error();

// needed by the debug tools to get the serr buffer of a thread.
// use serr_get_error if you want to clear and get the serr error.
std::shared_ptr<std::string> internal_get_serr_buffer();

void slog_raw(const char* msg, size_t len);
void serr_raw(const char* msg, size_t len);

void slog(const char* msg);
void serr(const char* msg);

void slogf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void serrf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// a possible improvement would be to use a std::vector as a parameter,
// to allow a potential reuse of memory.
std::unique_ptr<char[]> unique_vasprintf(int* length, const char* fmt, va_list args);
inline std::unique_ptr<char[]> unique_asprintf(int* length, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	std::unique_ptr<char[]> ret = unique_vasprintf(length, fmt, args);
	va_end(args);
	return ret;
}
