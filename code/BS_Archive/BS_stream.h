#pragma once

#include "BS_archive.h"
#include <cstdio>

// forward declaration
class RWops;

struct BS_MemoryStream
{
	typedef char Ch;

	const char* src; //!< Current read position.
	const char* head; //!< Original head of the string.
	const char* end;
	// I could replace this with something like end = NULL,
	// but I don't care that much about micro optimization.
	bool error;

	explicit BS_MemoryStream(const char* src_, const char* end_)
	: src(src_)
	, head(src_)
	, end(end_)
	, error(false)
	{
	}

	char Peek() const
	{
		return (src >= end ? '\0' : *src);
	}
	char Take()
	{
		// the comma operator is very important
		return (src >= end ? (error = true, '\0') : *src++);
	}
	size_t Tell() const
	{
		return src - head;
	}

	size_t Size() const
	{
		return end - head;
	}

	bool Rewind()
	{
		src = head;
		return true;
	}

	char* PutBegin()
	{
		ASSERT(false);
		return 0;
	}
	void Put(char)
	{
		ASSERT(false);
	}
	void Flush()
	{
		ASSERT(false);
	}
	size_t PutEnd(char*)
	{
		ASSERT(false);
		return 0;
	}

	bool good() const
	{
		return !error;
	}
};

#ifndef DISABLE_BS_JSON
#include <rapidjson/stringbuffer.h>

namespace rj = rapidjson;

// I have a feeling this is more optimized.
struct BS_StringBuffer : rj::StringBuffer
{
	explicit BS_StringBuffer(size_t capacity = kDefaultCapacity)
	: rj::StringBuffer(0, capacity)
	{
	}
	static const size_t kDefaultCapacity = 256;

	bool good() const
	{
		return true;
	}
};
#else
struct BS_StringBuffer
{
	typedef char Ch;

	std::ostringstream oss;
	std::string stringhold;

	size_t Size()
	{
		return oss.tellp();
	}

	void Flush() {}

	void Put(char c)
	{
		oss.put(c);
	}

	size_t GetLength()
	{
		if(stringhold.empty())
		{
			stringhold = oss.str();
		}
		return stringhold.size();
	}

	const char* GetString()
	{
		if(stringhold.empty())
		{
			stringhold = oss.str();
		}
		return stringhold.c_str();
	}

	char Peek() const
	{
		ASSERT(false);
		return '\0';
	}
	char Take()
	{
		ASSERT(false);
		return '\0';
	}
	size_t Tell() const
	{
		ASSERT(false);
		return 0;
	}

	char* PutBegin()
	{
		ASSERT(false);
		return 0;
	}
	size_t PutEnd(char*)
	{
		ASSERT(false);
		return 0;
	}

	bool good() const
	{
		return true;
	}
};
#endif

// This is copy pasted from rapidjson::FileReadStream, but modified for use by RWops.
class BS_ReadStream
{
public:
	typedef char Ch; //!< Character type (byte).

	BS_ReadStream(RWops* file_, char* buffer, size_t bufferSize)
	: file(file_)
	, buffer_(buffer)
	, bufferSize_(bufferSize)
	, bufferLast_(0)
	, current_(buffer_)
	, readCount_(0)
	, count_(0)
	, eof_(false)
	, error_(false)
	{
		Read();
	}

	Ch Peek() const
	{
		return *current_;
	}
	Ch Take()
	{
		Ch c = *current_;
		Read();
		return c;
	}
	size_t Tell() const
	{
		return count_ + (current_ - buffer_);
	}

	size_t Size() const;

	bool Rewind();

	// Not implemented
	void Put(Ch)
	{
		ASSERT(false);
	}
	void Flush()
	{
		ASSERT(false);
	}
	Ch* PutBegin()
	{
		ASSERT(false);
		return 0;
	}
	size_t PutEnd(Ch*)
	{
		ASSERT(false);
		return 0;
	}

	bool good() const
	{
		return !error_;
	}

private:
	void Read();

	RWops* file;
	Ch* buffer_;
	size_t bufferSize_;
	Ch* bufferLast_;
	Ch* current_;
	size_t readCount_;
	size_t count_; //!< Number of characters read
	bool eof_;
	bool error_;
};

// You might want to flush if you want RWops::tell to work.
// this is copy pasted from rapidjson::FileWriteStream
class BS_WriteStream
{
public:
	typedef char Ch; //!< Character type. Only support char.

	BS_WriteStream(RWops* file_, char* buffer, size_t bufferSize)
	: file(file_)
	, buffer_(buffer)
	, bufferEnd_(buffer + bufferSize)
	, current_(buffer_)
	, error(false)
	{
	}

	void Put(char c)
	{
		if(error) return;
		if(current_ >= bufferEnd_) Flush();
		*current_++ = c;
	}

	void Flush();

	// Not implemented
	char Peek() const
	{
		ASSERT(false);
		return 0;
	}
	char Take()
	{
		ASSERT(false);
		return 0;
	}

	char* PutBegin()
	{
		ASSERT(false);
		return 0;
	}
	size_t PutEnd(char*)
	{
		ASSERT(false);
		return 0;
	}

	bool good() const
	{
		return !error;
	}

private:
	RWops* file;
	char* buffer_;
	char* bufferEnd_;
	char* current_;
	bool error;
};

// convenience api, you could implement this yourself.
class BS_Serializable
{
public:
	virtual ~BS_Serializable() = default;
	virtual void Serialize(BS_Archive& ar) = 0;
};

enum
{
#ifndef DISABLE_BS_JSON
	BS_FLAG_JSON,
#endif
	BS_FLAG_BINARY
};

bool BS_Read_Memory(
	BS_Serializable& reader, const char* data, size_t size, int flags, const char* info = NULL);
bool BS_Read_Stream(BS_Serializable& reader, RWops* file, int flags, const char* info = NULL);
bool BS_Write_Memory(
	BS_Serializable& writer, BS_StringBuffer& sb, int flags, const char* info = NULL);
bool BS_Write_Stream(BS_Serializable& writer, RWops* file, int flags, const char* info = NULL);