#include "global_pch.h"
#include "global.h"

#include "cvar.h"

#include "debug_tools.h"

// disabling the console for emscripten wouldn't be that bad of an idea
// since I could make the console exist within the
#ifndef DISABLE_CONSOLE
#include "console.h"
#endif

#include <cstring>

static int has_stacktraces =
#if defined(HAS_STACKTRACE_PROBABLY)
	1;
#else
	0;
#endif

static REGISTER_CVAR_INT(
	cv_serr_bt,
	has_stacktraces,
	// I don't reccomend "always stacktrace" because some errors are nested,
	// which will make the error very hard to read.
	// a "capture" is a error that is handled, using serr_get_error()
	"0 = nothing, 1 = stacktrace (once per capture), 2 = always stacktrace (spam)",
	CVAR_T::RUNTIME);

// I would use this if I was profiling, or if the logs were spamming.
static REGISTER_CVAR_INT(cv_disable_log, 0, "0 = keep log, 2 = disable all logs", CVAR_T::RUNTIME);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

std::string WIN_WideToUTF8(const wchar_t* buffer, int size)
{
	int length = WideCharToMultiByte(CP_UTF8, 0, buffer, size, NULL, 0, NULL, NULL);
	std::string output(length, 0);
	WideCharToMultiByte(CP_UTF8, 0, buffer, size, output.data(), length, NULL, NULL);
	return output;
}

std::wstring WIN_UTF8ToWide(const char* buffer, int size)
{
	int length = MultiByteToWideChar(CP_UTF8, 0, buffer, size, NULL, 0);
	std::wstring output(length, 0);
	MultiByteToWideChar(CP_UTF8, 0, buffer, size, output.data(), length);
	return output;
}

std::string WIN_GetFormattedGLE()
{
	// Retrieve the system error message for the last-error code

	wchar_t* lpMsgBuf = NULL;
	DWORD dw = GetLastError();

	// It is possible to set the current codepage to utf8, and get utf8 messages from this
	// but then I would need to use unique_ptr<> with a custom deleter for LocalFree
	// to get the data without any redundant copying.
	int buflen = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_MAX_WIDTH_MASK, // this removes the extra newline
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPWSTR>(&lpMsgBuf),
		0,
		NULL);

	std::string str(WIN_WideToUTF8(lpMsgBuf, buflen));

	LocalFree(lpMsgBuf);

	return str;
}
#endif

bool __attribute__((noinline))
implement_CHECK(bool cond, const char* expr, const char* file, int line)
{
	if(!cond)
	{
		std::string stack_message;
		debug_str_stacktrace(&stack_message, 1);
		serrf(
			"\nCheck failed\n"
			"File: %s, Line %d\n"
			"Expression: `%s`\n"
			"\nStacktrace:\n"
			"%s\n",
			file,
			line,
			expr,
			stack_message.c_str());

		return false;
	}

	return true;
}

#if 0
#ifndef LOG_FILENAME
#define LOG_FILENAME "log.txt"
#endif

struct log_wrapper : nocopy
{
	FILE* fp = NULL;

	log_wrapper()
	{
#ifdef DISABLE_CONSOLE
		fp = fopen(LOG_FILENAME, "w");
#else
		// I need the w+ because I read in the in game console
		fp = fopen(LOG_FILENAME, "w+");
#endif
		if(fp == NULL)
		{
			printf("Failed to open log: `%s`, reason: %s\n", LOG_FILENAME, strerror(errno));
			return;
		}
	}

	~log_wrapper()
	{
		ASSERT(fp != NULL);
		int prev_error = ferror(fp);
		int ret = fclose(fp);
		fp = NULL;
		if(ret != 0 && prev_error != 0)
		{
			printf(
				"Failed to close log: `%s`, reason: %s (return: %d)\n",
				LOG_FILENAME,
				strerror(errno),
				ret);
		}
	}
};
FILE* get_global_log_file()
{
#ifdef DISABLE_LOG_FILE
	return NULL;
#else
	static log_wrapper log;
	return log.fp;
#endif
}
#endif

// serr buffer lazy initialized.
std::shared_ptr<std::string> internal_get_serr_buffer()
{
	static
#ifndef __EMSCRIPTEN__
		thread_local
#endif
		std::shared_ptr<std::string>
			buffer;
	if(!buffer)
	{
		buffer = std::make_shared<std::string>();
	}
	return buffer;
}

static void __attribute__((noinline)) serr_safe_stacktrace(int skip = 0)
{
	// NOTE: I am thinking of making the stacktrace only appear in stdout
	// and in the console error section.
	(void)skip;
	if(cv_serr_bt.data == 2 || (cv_serr_bt.data == 1 && !serr_check_error()))
	{
		std::string msg;
		{
			// this could be avoided with c++20's string allocator callback thing
			// but I don't want to use c++20.
			int length;
			std::unique_ptr<char[]> buffer;
			buffer = unique_asprintf(
				&length, "StackTrace (%s = %d)\n", cv_serr_bt.cvar_key, cv_serr_bt.data);
			if(buffer)
			{
				msg.assign(buffer.get(), length);
			}
		}
		debug_str_stacktrace(&msg, skip + 1);
		msg += '\n';

		internal_get_serr_buffer()->append(msg);
		fwrite(msg.c_str(), 1, msg.size(), stdout);

		{
#ifndef __EMSCRIPTEN__
			std::lock_guard<std::mutex> lk(g_log_mut);
#endif
            g_log.push(CONSOLE_MESSAGE_TYPE::ERROR, msg.c_str(), msg.size());
		}
	}
}

std::string serr_get_error()
{
	size_t max_size = 10000;
	if(internal_get_serr_buffer()->size() > max_size)
	{
		slogf(
			"info: %s truncating message (size: %zu, max: %zu)\n",
			__func__,
			internal_get_serr_buffer()->size(),
			max_size);
		internal_get_serr_buffer()->resize(max_size);
	}
	return std::move(*internal_get_serr_buffer());
}
bool serr_check_error()
{
	return !internal_get_serr_buffer()->empty();
}

void slog_raw(const char* msg, size_t len)
{
	ASSERT(msg != NULL);
	ASSERT(len != 0);
	if(cv_disable_log.data != 0)
	{
		return;
	}
	// on win32, if did a /subsystem:windows, I would probably
	// replace stdout with OutputDebugString on the debug build.
	fwrite(msg, 1, len, stdout);
	{
#ifndef __EMSCRIPTEN__
		std::lock_guard<std::mutex> lk(g_log_mut);
#endif
        g_log.push(CONSOLE_MESSAGE_TYPE::INFO, msg, len);
	}
}
void serr_raw(const char* msg, size_t len)
{
	ASSERT(msg != NULL);
	ASSERT(len != 0);
	if(cv_disable_log.data != 0)
	{
		// if I didn't do this, there would be side effects
		// since I sometimes depend on serr_check_error for checking.
		*internal_get_serr_buffer() = '!';
		return;
	}
	serr_safe_stacktrace(1);

	internal_get_serr_buffer()->append(msg, msg + len);
	fwrite(msg, 1, len, stdout);

	{
#ifndef __EMSCRIPTEN__
		std::lock_guard<std::mutex> lk(g_log_mut);
#endif
        g_log.push(CONSOLE_MESSAGE_TYPE::ERROR, msg, len);
	}
}

void slog(const char* msg)
{
	slog_raw(msg, strlen(msg));
}

void serr(const char* msg)
{
	serr_raw(msg, strlen(msg));
}

void slogf(const char* fmt, ...)
{
	ASSERT(fmt != NULL);
	if(cv_disable_log.data != 0)
	{
		return;
	}
	va_list args;
	va_list temp_args;
	va_start(args, fmt);
	va_copy(temp_args, args);

#ifdef WIN32
	// win32 has a compatible C standard library, but annex k prevents exploits or something.
	vfprintf_s(stdout, fmt, args);
#else
	vfprintf(stdout, fmt, args);
#endif
	va_end(args);
	va_start(temp_args, fmt);

	{
#ifndef __EMSCRIPTEN__
		std::lock_guard<std::mutex> lk(g_log_mut);
#endif
		g_log.push_vargs(CONSOLE_MESSAGE_TYPE::INFO, fmt, temp_args);
	}
	va_end(temp_args);
}

void serrf(const char* fmt, ...)
{
	ASSERT(fmt != NULL);
	if(cv_disable_log.data != 0)
	{
		// if I didn't do this, there would be side effects
		// since I sometimes depend on serr_check_error for checking.
		*internal_get_serr_buffer() = '!';
		return;
	}

	serr_safe_stacktrace(1);

	va_list args;
	va_list temp_args;
	va_start(args, fmt);
	va_copy(temp_args, args);

#ifdef WIN32
	// win32 has a compatible C standard library, but annex k prevents exploits or something.
	vfprintf_s(stdout, fmt, args);
#else
	vfprintf(stdout, fmt, args);
#endif
	va_end(args);

    int len;
	va_start(temp_args, fmt);
    std::unique_ptr<char[]> buffer = unique_vasprintf(&len, fmt, temp_args);
	va_end(temp_args);
	internal_get_serr_buffer()->append(buffer.get(), buffer.get() + len);
	{
#ifndef __EMSCRIPTEN__
		std::lock_guard<std::mutex> lk(g_log_mut);
#endif
		g_log.push(CONSOLE_MESSAGE_TYPE::ERROR, buffer.get(), len);
	}


#if 0
		int ret;
#ifdef WIN32
		// win32 has a compatible C standard library, but annex k prevents exploits or something.
		ret = vfprintf_s(get_global_log_file(), fmt, temp_args);
#else
		ret = vfprintf(get_global_log_file(), fmt, temp_args);
#endif
#endif
}

std::unique_ptr<char[]> unique_vasprintf(int* length, const char* fmt, va_list args)
{
	ASSERT(fmt != NULL);
	int ret;
	va_list temp_args;
	std::unique_ptr<char[]> buffer;

	// it says you should copy if you use valist more than once.
	va_copy(temp_args, args);
#ifdef WIN32
	// win32 has a compatible C standard library, but annex k prevents exploits or something.
	ret = _vscprintf(fmt, temp_args);
#else
	ret = vsnprintf(NULL, 0, fmt, temp_args);
#endif
	va_end(temp_args);

	ASSERT(ret != -1);
	if(ret == -1) goto err;

	buffer.reset(new char[ret + 1]);

#ifdef WIN32
	ret = vsprintf_s(buffer.get(), ret + 1, fmt, args);
#else
	ret = vsnprintf(buffer.get(), ret + 1, fmt, args);
#endif

	ASSERT(ret != -1);
	if(ret == -1) goto err;

	if(length != NULL)
	{
		*length = ret;
	}

	return buffer;

err:
	static char badformatmessage[] = "??";
	buffer.reset(new char[sizeof(badformatmessage)]);
	memcpy(buffer.get(), badformatmessage, sizeof(badformatmessage));
	if(length != NULL)
	{
		*length = sizeof(badformatmessage) - 1;
	}
	return buffer;
}
