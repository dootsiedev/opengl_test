#include "global.h"
#include "cvar.h"
#include "debug_tools.h"

#ifndef DISABLE_CONSOLE
#include "console.h"
#endif

#include <cstring>

// USE_LIBBACKTRACE should imply debug information exists.
static int has_libbacktrace =
#if defined(USE_LIBBACKTRACE)
	1;
#else
	0;
#endif

static REGISTER_CVAR_INT(
	cv_serr_bt,
	has_libbacktrace,
	// I don't reccomend "always stacktrace" because some errors are nested,
	// which will make the error very hard to read.
	// a "capture" is a error that is handled, using serr_get_error()
	"0 = nothing, 1 = stacktrace (once per capture), 2 = always stacktrace (spam)",
	CVAR_T::RUNTIME);

// I would use this if I was profiling, or if the logs were spamming.
static REGISTER_CVAR_INT(
	cv_disable_log,
	0,
	"0 = keep log, 1 = disable info logs, 2 = disable info and error logs",
	CVAR_T::RUNTIME);

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

// serr buffer lazy initialized.
std::shared_ptr<std::string> internal_get_serr_buffer()
{
	static thread_local std::shared_ptr<std::string> buffer;
	if(!buffer)
	{
		buffer = std::make_shared<std::string>();
	}
	return buffer;
}

static void __attribute__((noinline)) serr_safe_stacktrace(int skip = 0)
{
	(void)skip;
	if(cv_serr_bt.data == 2 || (cv_serr_bt.data == 1 && !serr_check_error()))
	{
		int length;
		std::unique_ptr<char[]> buffer;
		buffer = unique_asprintf(
			&length, "StackTrace (%s = %d)\n", cv_serr_bt.cvar_key, cv_serr_bt.data);
		std::string msg;
		if(buffer)
		{
			msg.assign(buffer.get(), length);
		}

		debug_str_stacktrace(&msg, skip + 1);
		msg += '\n';
		slog_raw(msg.data(), msg.length());
		internal_get_serr_buffer()->append(msg);
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
	if(cv_disable_log.data != 0)
	{
		return;
	}
	// on win32, if did a /subsystem:windows, I would probably
	// replace stdout with OutputDebugString on the debug build.
	fwrite(msg, 1, len, stdout);
#ifndef DISABLE_CONSOLE
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len);
	memcpy(buffer.get(), msg, len);
	{
		std::lock_guard<std::mutex> lk(g_console.mut);
		g_console.message_queue.emplace_back(CONSOLE_MESSAGE_TYPE::INFO, std::move(buffer), len);
	}
#endif
}
void serr_raw(const char* msg, size_t len)
{
	if(cv_disable_log.data == 2)
	{
		// if I didn't do this, there would be side effects
		// since I sometimes depend on serr_check_error for checking.
		*internal_get_serr_buffer() = '!';
		return;
	}
	serr_safe_stacktrace(1);
	slog_raw(msg, len);
	internal_get_serr_buffer()->append(msg, msg + len);
}

void slog(const char* msg)
{
	if(cv_disable_log.data != 0)
	{
		return;
	}
#ifdef DISABLE_CONSOLE
	// don't use puts because it inserts a newline.
	fputs(msg, stdout);
#else
	slog_raw(msg, strlen(msg));
#endif
}

void serr(const char* msg)
{
	if(cv_disable_log.data == 2)
	{
		// if I didn't do this, there would be side effects
		// since I sometimes depend on serr_check_error for checking.
		*internal_get_serr_buffer() = '!';
		return;
	}
	serr_safe_stacktrace(1);
	slog(msg);
	internal_get_serr_buffer()->append(msg);
}

void slogf(const char* fmt, ...)
{
	if(cv_disable_log.data != 0)
	{
		return;
	}
#ifdef DISABLE_CONSOLE
	va_list args;
	va_start(args, fmt);
#ifdef WIN32
	// win32 has a compatible C standard library, but annex k prevents exploits or something.
	vfprintf_s(stdout, fmt, args);
#else
	vfprintf(stdout, fmt, args);
#endif
	va_end(args);
#else
	int length;
	va_list args;
	va_start(args, fmt);
	std::unique_ptr<char[]> buffer = unique_vasprintf(&length, fmt, args);
	va_end(args);
	fwrite(buffer.get(), 1, length, stdout);
	{
		std::lock_guard<std::mutex> lk(g_console.mut);
		g_console.message_queue.emplace_back(CONSOLE_MESSAGE_TYPE::INFO, std::move(buffer), length);
	}
#endif
}

void serrf(const char* fmt, ...)
{
	if(cv_disable_log.data == 2)
	{
		// if I didn't do this, there would be side effects
		// since I sometimes depend on serr_check_error for checking.
		*internal_get_serr_buffer() = '!';
		return;
	}

	serr_safe_stacktrace(1);

	int length;
	va_list args;

	va_start(args, fmt);
	std::unique_ptr<char[]> buffer = unique_vasprintf(&length, fmt, args);
	va_end(args);

	internal_get_serr_buffer()->append(buffer.get(), buffer.get() + length);

#ifdef DISABLE_CONSOLE
	// would it be much faster if I added vslogf and did a va_copy?
	slog_raw(buffer.get(), length);
#else
	fwrite(buffer.get(), 1, length, stdout);
	{
		std::lock_guard<std::mutex> lk(g_console.mut);
		g_console.message_queue.emplace_back(CONSOLE_MESSAGE_TYPE::INFO, std::move(buffer), length);
	}
#endif
}

std::unique_ptr<char[]> unique_vasprintf(int* length, const char* fmt, va_list args)
{
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
