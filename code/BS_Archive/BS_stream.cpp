#include "../global_pch.h"
#include "../global.h"
#include "BS_stream.h"
#include "../RWops.h"
#ifndef DISABLE_BS_JSON
#include "BS_json.h"
#endif
#include "BS_binary.h"

void BS_ReadStream::Read()
{
	if(current_ < bufferLast_)
	{
		++current_;
	}
	else if(!eof_)
	{
		count_ += readCount_;
		readCount_ = file->read(buffer_, 1, bufferSize_);
		bufferLast_ = buffer_ + readCount_ - 1;
		current_ = buffer_;

		if(readCount_ < bufferSize_)
		{
			buffer_[readCount_] = '\0';
			++bufferLast_;
			eof_ = true;
		}
	}
	else
	{
		error_ = true;
	}
}

size_t BS_ReadStream::Size() const
{
	RW_ssize_t old_cur = file->tell();
	if(old_cur < 0)
	{
		return 0;
	}
	if(file->seek(0, SEEK_END) < 0)
	{
		return 0;
	}

	RW_ssize_t end_pos = file->tell();
	if(end_pos < 0)
	{
		return 0;
	}

	if(file->seek(old_cur, SEEK_SET) < 0)
	{
		return 0;
	}

	return end_pos;
}

bool BS_ReadStream::Rewind()
{
	current_ = buffer_;
	count_ = 0;
	eof_ = false;
	error_ = false;
	readCount_ = 0;
	bufferLast_ = 0;
	if(file->seek(0, SEEK_SET) < 0)
	{
		return false;
	}
	Read();
	return true;
}

void BS_WriteStream::Flush()
{
	if(current_ != buffer_)
	{
		size_t result = file->write(buffer_, 1, current_ - buffer_);
		if(result < static_cast<size_t>(current_ - buffer_))
		{
			error = true;
		}
		current_ = buffer_;
	}
}

bool BS_Read_Memory(
	BS_Serializable& reader, const char* data, size_t size, int flags, const char* info)
{
	BS_MemoryStream sb(data, data + size);
	switch(flags)
	{
#ifndef DISABLE_BS_JSON
	case BS_FLAG_JSON: {
		BS_JsonReader ar(sb);
		reader.Serialize(ar);
		return ar.Finish(info);
	}
#endif
	case BS_FLAG_BINARY: {
		BS_BinaryReader ar(sb);
		reader.Serialize(ar);
		return ar.Finish(info);
	}
	}
	ASSERT(false && "unreachable");
	return false;
}
bool BS_Read_Stream(BS_Serializable& reader, RWops* file, int flags, const char* info)
{
	info = (info == NULL) ? file->name() : info;
	char buffer[1000];
	BS_ReadStream sb(file, buffer, sizeof(buffer));
	switch(flags)
	{
#ifndef DISABLE_BS_JSON
	case BS_FLAG_JSON: {
		BS_JsonReader ar(sb);
		reader.Serialize(ar);
		return ar.Finish(info);
	}
#endif
	case BS_FLAG_BINARY: {
		BS_BinaryReader ar(sb);
		reader.Serialize(ar);
		return ar.Finish(info);
	}
	}
	ASSERT(false && "unreachable");
	return false;
}
bool BS_Write_Memory(BS_Serializable& writer, BS_StringBuffer& sb, int flags, const char* info)
{
	switch(flags)
	{
#ifndef DISABLE_BS_JSON
	case BS_FLAG_JSON: {
		BS_JsonWriter ar(sb);
		writer.Serialize(ar);
		return ar.Finish(info);
	}
#endif
	case BS_FLAG_BINARY: {
		BS_BinaryWriter ar(sb);
		writer.Serialize(ar);
		return ar.Finish(info);
	}
	}
	ASSERT(false && "unreachable");
	return false;
}
bool BS_Write_Stream(BS_Serializable& writer, RWops* file, int flags, const char* info)
{
	info = (info == NULL) ? file->name() : info;
	char buffer[1000];
	BS_WriteStream sb(file, buffer, sizeof(buffer));
	switch(flags)
	{
#ifndef DISABLE_BS_JSON
	case BS_FLAG_JSON: {
		BS_JsonWriter ar(sb);
		writer.Serialize(ar);
		return ar.Finish(info);
	}
#endif
	case BS_FLAG_BINARY: {
		BS_BinaryWriter ar(sb);
		writer.Serialize(ar);
		return ar.Finish(info);
	}
	}
	ASSERT(false && "unreachable");
	return false;
}