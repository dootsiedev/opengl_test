#include "global_pch.h"
#include "global.h"

#include "RWops.h"

#include <cstdio>
#include <limits>
#include <string.h> //strerror
#include <errno.h> //errno

#include <SDL2/SDL.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
// this is annoying but I would rather do this than suppress warnings
#define _fileno fileno
#define _fstat fstat
#define _stat stat
#endif

// assumptions
static_assert(SEEK_SET == RW_SEEK_SET);
static_assert(SEEK_CUR == RW_SEEK_CUR);
static_assert(SEEK_END == RW_SEEK_END);

// I probably want to support zip files
// should remove SDL dependance when I start accessing RWops from other threads,
// (but I kinda like keeping SDL because of the android archive support, but oh well)

RWops_Stdio::RWops_Stdio(FILE* stream, std::string file)
: stream_name(std::move(file))
, fp(stream)
{
	ASSERT(stream != NULL);
}

const char* RWops_Stdio::name()
{
	return stream_name.c_str();
}
size_t RWops_Stdio::read(void* ptr, size_t size, size_t nmemb)
{
	ASSERT(fp != NULL);
	size_t bytes_read = fread(ptr, size, nmemb, fp);
	if(bytes_read != size && ferror(fp) != 0)
	{
		serrf(
			"Error reading from datastream: `%s`, reason: %s (return: %zu)\n",
			stream_name.c_str(),
			strerror(errno),
			bytes_read);
	}
	return bytes_read;
}
size_t RWops_Stdio::write(const void* ptr, size_t size, size_t nmemb)
{
	ASSERT(fp != NULL);
	size_t bytes_written = fwrite(ptr, size, nmemb, fp);
	if(bytes_written != size && ferror(fp) != 0)
	{
		serrf(
			"Error writing to datastream: `%s`, reason: %s (return: %zu)\n",
			stream_name.c_str(),
			strerror(errno),
			bytes_written);
	}
	return bytes_written;
}
int RWops_Stdio::seek(RW_ssize_t offset, int whence)
{
	ASSERT(fp != NULL);
	int ret = fseek(fp, offset, whence);
	if(ret != 0)
	{
		serrf(
			"Error seeking in datastream: `%s`, reason: %s (return: %d)\n",
			stream_name.c_str(),
			strerror(errno),
			ret);
	}
	return ret;
}
RW_ssize_t RWops_Stdio::tell()
{
	ASSERT(fp != NULL);
	// spec says this doesn't have any specific errno codes on error
	// not to say the error indicator could cause an error.
	return ftell(fp);
}
RW_ssize_t RWops_Stdio::size()
{
	ASSERT(fp != NULL);
	// spec says this doesn't have any specific errno codes on error
	// not to say the error indicator could cause an error.
	struct _stat info;
	int ret = _fstat(_fileno(fp), &info);
	if(ret != 0)
	{
		serrf(
			"Error getting datastream size: `%s`, reason: %s (return: %d)\n",
			stream_name.c_str(),
			strerror(errno),
			ret);
		return -1;
	}
	return info.st_size;
}
bool RWops_Stdio::close()
{
	ASSERT(fp != NULL);
	int prev_error = ferror(fp);
	int ret = fclose(fp);
	fp = NULL;
	if(ret != 0 && prev_error != 0)
	{
		serrf(
			"Failed to close: `%s`, reason: %s (return: %d)\n",
			stream_name.c_str(),
			strerror(errno),
			ret);
		return false;
	}
	return true;
}
RWops_Stdio::~RWops_Stdio()
{
	if(fp != NULL)
	{
		slogf("info: file destroyed without closing: %s\n", stream_name.c_str());
		fclose(fp);
		fp = NULL;
	}
}

// this is how I implement the buffer API because I am too lazy to copy paste the code.
// not high performance by any means, but portable.
class RWops_SDL : public RWops
{
public:
	std::string stream_name;
	SDL_RWops* sdl_ops;
	RWops_SDL(SDL_RWops* stream, std::string file)
	: stream_name(std::move(file))
	, sdl_ops(stream)
	{
		ASSERT(stream != NULL);
	}

	const char* name() override
	{
		return stream_name.c_str();
	}

	size_t read(void* ptr, size_t size, size_t nmemb) override
	{
		ASSERT(sdl_ops != NULL);
		SDL_ClearError();
		size_t bytes_read = SDL_RWread(sdl_ops, ptr, size, nmemb);
		const char* error = SDL_GetError();
		// SDL spec says that to check for non eof error you must compare GetError with an empty
		// string to detect errors.
		if(bytes_read != size && error[0] != '\0')
		{
			// errno is not set by fread, if I really wanted to I could use open() and read(), but
			// that is too painful.
			serrf(
				"Error reading from datastream: `%s`, reason: %s (return: %zu)\n",
				stream_name.c_str(),
				error,
				bytes_read);
		}
		return bytes_read;
	}
	size_t write(const void* ptr, size_t size, size_t nmemb) override
	{
		ASSERT(sdl_ops != NULL);
		SDL_ClearError();
		size_t bytes_written = SDL_RWwrite(sdl_ops, ptr, size, nmemb);
		const char* error = SDL_GetError();
		if(bytes_written != size && error[0] != '\0')
		{
			serrf(
				"Error writing to datastream: `%s`, reason: %s (return: %zu)\n",
				stream_name.c_str(),
				error,
				bytes_written);
		}
		return bytes_written;
	}
	int seek(RW_ssize_t offset, int whence) override
	{
		ASSERT(sdl_ops != NULL);
		// SDL returns tell(), but stdio uses zero as an error check.
		Sint64 out = SDL_RWseek(sdl_ops, offset, whence);
		if(out < 0)
		{
			const char* error = SDL_GetError();
			serrf(
				"Error seeking in datastream: `%s`, reason: %s (return: %" PRId64 ")\n",
				stream_name.c_str(),
				error,
				out);
			return -1;
		}
		return 0;
	}
	RW_ssize_t tell() override
	{
		ASSERT(sdl_ops != NULL);
		Sint64 ret = SDL_RWtell(sdl_ops);
		if(ret < 0)
		{
			const char* error = SDL_GetError();
			serrf(
				"Error calling tell in datastream: `%s`, reason: %s (return: %" PRId64 ")\n",
				stream_name.c_str(),
				error,
				ret);
		}
		return ret;
	}
	RW_ssize_t size() override
	{
		ASSERT(sdl_ops != NULL);
		Sint64 ret = SDL_RWsize(sdl_ops);
		if(ret < 0)
		{
			const char* error = SDL_GetError();
			serrf(
				"Error getting datastream size: `%s`, reason: %s (return: %" PRId64 ")\n",
				stream_name.c_str(),
				error,
				ret);
			return -1;
		}
		return ret;
	}
	bool close() override
	{
		ASSERT(sdl_ops != NULL);
		int out = SDL_RWclose(sdl_ops);
		sdl_ops = NULL;
		if(out != 0)
		{
			const char* error = SDL_GetError();
			serrf(
				"Failed to close: `%s`, reason: %s (return: %d)\n",
				stream_name.c_str(),
				error,
				out);
			return false;
		}
		return true;
	}
	~RWops_SDL() override
	{
		if(sdl_ops != NULL)
		{
			slogf("info: file destroyed without closing: %s\n", stream_name.c_str());
			SDL_RWclose(sdl_ops);
			sdl_ops = NULL;
		}
	}
};

FILE* serr_wrapper_fopen(const char* path, const char* mode)
{
	FILE* fp = fopen(path, mode);
	if(fp == NULL)
	{
		serrf("Failed to open: `%s`, reason: %s\n", path, strerror(errno));
	}
	return fp;
}

Unique_RWops Unique_RWops_OpenFS(std::string path, const char* mode)
{
	FILE* fp = serr_wrapper_fopen(path.c_str(), mode);
	if(fp == NULL)
	{
		return Unique_RWops();
	}
	return std::make_unique<RWops_Stdio>(fp, std::move(path));
}
Unique_RWops Unique_RWops_FromFP(FILE* fp, std::string name)
{
	return std::make_unique<RWops_Stdio>(fp, std::move(name));
}
Unique_RWops Unique_RWops_FromMemory(char* memory, size_t size, bool readonly, std::string name)
{
	if(size > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		serrf("Failed to open: `%s`, reason: size too large: %zu\n", name.c_str(), size);
		return Unique_RWops();
	}
	SDL_RWops* sdl_rwop =
		(readonly ? SDL_RWFromConstMem(memory, static_cast<int>(size))
				  : SDL_RWFromMem(memory, static_cast<int>(size)));
	if(sdl_rwop == NULL)
	{
		const char* error = SDL_GetError();
		serrf("Failed to open: `%s`, reason: %s\n", name.c_str(), error);
		return Unique_RWops();
	}
	return std::make_unique<RWops_SDL>(sdl_rwop, std::move(name));
}

/*
this is dead code, I might revive one day.

//The only problem with this is that you have no error information,
//so use Unique_RWops_Close() which will print an error and close.
struct RWops_Deleter
{
	void operator()(SDL_RWops* ops){
		if(SDL_RWclose(ops) < 0)
		{
			ASSERT(false && "SDL_RWclose");
		}
	}
};

typedef std::unique_ptr<SDL_RWops, RWops_Deleter> Unique_RWops;

MYNODISCARD Unique_RWops Unique_RWops_OpenFS(const char* path, const char* mode);

//closes the RWops with an error message.
//returns false on error.
MYNODISCARD bool Unique_RWops_Close(Unique_RWops& file, const char* msg = "<unspecified>");

Unique_RWops Unique_RWops_OpenFS(const char* path, const char* mode)
{
	// I prefer using SDL_RWFromFP instead of SDL_RWFromFile because on windows sdl will open a
	// win32 HANDLE, and SDL will print the path of the file inside of SDL_GerError,
	// and the message excludes any specific error like errno/GLE.
	//
	// Note if you comple SDL2 on windows from source
	// by default the cmake flag LIBC is off by default
	// which will cause SDL_RWFromFP to fail due to missing stdio.h,
	// but this means you must link to the correct CRT runtimes.
	//
	//TODO: I should just copy paste the SDL2 implementation of stdio, and split it between readonly
and writeonly
	//and make it autoamtically print errno to serr for functions like RWread which only ruturn 0
	FILE* fp = fopen(path, mode);
	if(!fp)
	{
		serrf("Failed to open: `%s`, reason: %s\n", path, strerror(errno));
		return Unique_RWops();
	}
	// SDL_TRUE for closing the FILE* stream automatically.
	SDL_RWops* rwop = SDL_RWFromFP(fp, SDL_TRUE);
	if(!rwop)
	{
		serrf("SDL_RWFromFP %s: %s\n"
		"(there is only one way this could fail, which is if you compiled SDL2 without
-DLIBC=ON)\n", path, SDL_GetError()); return Unique_RWops();
	}
	return Unique_RWops(rwop);
}

bool Unique_RWops_Close(Unique_RWops& file, const char* msg)
{
	if(!file)
		return true;

	//release leaks the memory, unlike reset.
	SDL_RWops* sdl_ops = file.release();
	if(SDL_RWclose(sdl_ops) < 0)
	{
		//SDL_GetErrorMsg(buf, len) is thread safe, but a very recent addition to sdl
		serrf("Failed to close `%s`, reason: %s\n", msg, SDL_GetError());
		return false;
	}
	return true;
}
*/