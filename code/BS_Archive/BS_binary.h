#pragma once
#include "BS_archive.h"
#include <cstdint>

template<class InputStream>
class BS_BinaryReader : public BS_Archive
{
public:
	InputStream& stream;
	bool error = false;

	explicit BS_BinaryReader(InputStream& stream_)
	: stream(stream_)
	{
	}

	bool IsReader() const override
	{
		return true;
	}
	bool IsWriter() const override
	{
		return false;
	}
	bool Good() const override
	{
		return !error;
	}
	bool Finish(const char* info) override
	{
		info = (info == NULL) ? "<unspecified>" : info;
		if(stream.Tell() != stream.Size() || !stream.good())
		{
			serrf(
				"Failed to parse binary: %s\n"
				"Size: %zu\n"
				"Cursor: %zu\n",
				info,
				stream.Size(),
				stream.Tell());
			return false;
		}
		return true;
	}
	bool Bool(bool& b) override
	{
		return Bool_CB({}, internal_read_simple_cb<bool>, &b);
	}
	bool Bool_CB(bool b, BS_bool_cb cb, void* ud) override
	{
		(void)b; // unused
		if(error) return false;
		bool tmp = stream.Take();
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Int8(int8_t& i) override
	{
		return Int8_CB({}, internal_read_simple_cb<int8_t>, &i);
	}
	bool Int8_CB(int8_t i, BS_int8_cb cb, void* ud) override
	{
		(void)i; // unused
		if(error) return false;
		int8_t tmp = stream.Take();
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Int16(int16_t& i) override
	{
		return Int16_CB({}, internal_read_simple_cb<int16_t>, &i);
	}
	bool Int16_CB(int16_t i, BS_int16_cb cb, void* ud) override
	{
		(void)i; // unused
		if(error) return false;
		uint16_t tmp = static_cast<uint16_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(static_cast<int16_t>(tmp), ud);
		return !error;
	}
	bool Int32(int32_t& i) override
	{
		return Int32_CB({}, internal_read_simple_cb<int32_t>, &i);
	}
	bool Int32_CB(int32_t i, BS_int32_cb cb, void* ud) override
	{
		(void)i; // unused
		if(error) return false;
		uint32_t tmp = static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 24;
		tmp |= static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 16;
		tmp |= static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(static_cast<int32_t>(tmp), ud);
		return !error;
	}
	bool Int64(int64_t& i) override
	{
		return Int64_CB({}, internal_read_simple_cb<int64_t>, &i);
	}
	bool Int64_CB(int64_t i, BS_int64_cb cb, void* ud) override
	{
		(void)i; // unused
		if(error) return false;
		uint64_t tmp = static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 56;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 48;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 40;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 32;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 24;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 16;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(static_cast<int64_t>(tmp), ud);
		return !error;
	}
	bool Uint8(uint8_t& u) override
	{
		return Uint8_CB({}, internal_read_simple_cb<uint8_t>, &u);
	}
	bool Uint8_CB(uint8_t u, BS_uint8_cb cb, void* ud) override
	{
		(void)u; // unused
		if(error) return false;
		uint8_t tmp = static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Uint16(uint16_t& u) override
	{
		return Uint16_CB({}, internal_read_simple_cb<uint16_t>, &u);
	}
	bool Uint16_CB(uint16_t u, BS_uint16_cb cb, void* ud) override
	{
		(void)u; // unused
		if(error) return false;
		uint16_t tmp = static_cast<uint16_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Uint32(uint32_t& u) override
	{
		return Uint32_CB({}, internal_read_simple_cb<uint32_t>, &u);
	}
	bool Uint32_CB(uint32_t u, BS_uint32_cb cb, void* ud) override
	{
		(void)u; // unused
		if(error) return false;
		uint32_t tmp = static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 24;
		tmp |= static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 16;
		tmp |= static_cast<uint32_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Uint64(uint64_t& u) override
	{
		return Uint64_CB({}, internal_read_simple_cb<uint64_t>, &u);
	}
	bool Uint64_CB(uint64_t u, BS_uint64_cb cb, void* ud) override
	{
		(void)u; // unused
		if(error) return false;
		uint64_t tmp = static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 56;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 48;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 40;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 32;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 24;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 16;
		tmp |= static_cast<uint64_t>(static_cast<uint8_t>(stream.Take())) << 8;
		tmp |= static_cast<uint8_t>(stream.Take());
		error = error || !stream.good() || !cb(tmp, ud);
		return !error;
	}
	bool Float(float& d) override
	{
		return Float_CB({}, internal_read_simple_cb<float>, &d);
	}
	bool Float_CB(float d, BS_float_cb cb, void* ud) override
	{
		(void)d; // unused
		uint32_t tmp;
		if(!Uint32(tmp))
		{
			return false;
		}

		// this feels wrong, but I can't do
		// double d = *reinterpret_cast<double*>(&tmp);
		// because -Wstrict-aliasing warning
		float fhold;
		memcpy(&fhold, &tmp, sizeof(tmp));

		if(!std::isfinite(fhold))
		{
			serrf("invalid float: %f\n", fhold);
			error = true;
			return false;
		}
		error = error || !cb(fhold, ud);
		return !error;
	}
	bool Double(double& d) override
	{
		return Double_CB({}, internal_read_simple_cb<double>, &d);
	}
	bool Double_CB(double d, BS_double_cb cb, void* ud) override
	{
		(void)d; // unused
		uint64_t tmp;
		if(!Uint64(tmp))
		{
			return false;
		}

		// this feels wrong, but I can't do
		// double d = *reinterpret_cast<double*>(&tmp);
		// because -Wstrict-aliasing warning
		double fhold;
		memcpy(&fhold, &tmp, sizeof(tmp));

		if(!std::isfinite(fhold))
		{
			serrf("invalid double: %f\n", fhold);
			error = true;
			return false;
		}
		error = error || !cb(fhold, ud);
		return !error;
	}
	bool String(std::string& str) override
	{
		return StringZ_CB(str, BS_MAX_STRING_SIZE, internal_read_string_cb, &str);
	}
	bool String_CB(std::string_view str, BS_string_cb cb, void* ud) override
	{
		return StringZ_CB(str, BS_MAX_STRING_SIZE, cb, ud);
	}
	bool StringZ(std::string& str, size_t max_size) override
	{
		return StringZ_CB(str, max_size, internal_read_string_cb, &str);
	}
	bool StringZ_CB(std::string_view str, size_t max_size, BS_string_cb cb, void* ud) override
	{
		(void)str; // unused
		ASSERT(max_size <= BS_MAX_STRING_SIZE);
		char buf[BS_MAX_STRING_SIZE + 1];
		uint16_t size;

		if(!Uint16(size))
		{
			return false;
		}
		if(size > max_size)
		{
			error = true;
			serrf("string too large, max: %zu result: %u\n", max_size, size);
			return false;
		}
		size_t i;
		for(i = 0; i < size; ++i)
		{
			buf[i] = stream.Take();
		}
		buf[i] = '\0';

		error = error || !stream.good() || !cb(buf, size, ud);
		return !error;
	}
	bool Key(std::string_view str) override
	{
		(void)str; // unused
		return !error;
	}
	bool StartObject() override
	{
		return !error;
	}
	bool EndObject() override
	{
		return !error;
	}
	bool StartArray() override
	{
		return !error;
	}
	bool EndArray() override
	{
		return !error;
	}
};

template<class OutputStream>
class BS_BinaryWriter : public BS_Archive
{
public:
	OutputStream& stream;

	explicit BS_BinaryWriter(OutputStream& stream_)
	: stream(stream_)
	{
	}

	bool IsReader() const override
	{
		return false;
	}
	bool IsWriter() const override
	{
		return true;
	}
	bool Good() const override
	{
		return stream.good();
	}
	bool Finish(const char* info) override
	{
		info = (info == NULL) ? "<unspecified>" : info;
		stream.Flush();
		if(!stream.good())
		{
			serrf("Failed to write binary: %s\n", info);
			return false;
		}
		return true;
	}
	bool Bool(bool& b) override
	{
		stream.Put(b);
		return stream.good();
	}
	bool Bool_CB(bool b, BS_bool_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Bool(b);
	}
	bool Int8(int8_t& i) override
	{
		uint8_t tmp = static_cast<uint8_t>(i);
		return Uint8(tmp);
	}
	bool Int8_CB(int8_t i, BS_int8_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Int8(i);
	}
	bool Int16(int16_t& i) override
	{
		uint16_t tmp = static_cast<uint16_t>(i);
		return Uint16(tmp);
	}
	bool Int16_CB(int16_t i, BS_int16_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Int16(i);
	}
	bool Int32(int32_t& i) override
	{
		uint32_t tmp = static_cast<uint32_t>(i);
		return Uint32(tmp);
	}
	bool Int32_CB(int32_t i, BS_int32_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Int32(i);
	}
	bool Int64(int64_t& i) override
	{
		uint64_t tmp = static_cast<uint64_t>(i);
		return Uint64(tmp);
	}
	bool Int64_CB(int64_t i, BS_int64_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Int64(i);
	}
	bool Uint8(uint8_t& u) override
	{
		stream.Put(static_cast<char>(u));
		return stream.good();
	}
	bool Uint8_CB(uint8_t u, BS_uint8_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Uint8(u);
	}
	bool Uint16(uint16_t& u) override
	{
		stream.Put(static_cast<char>(u >> 8));
		stream.Put(static_cast<char>(u));
		return stream.good();
	}
	bool Uint16_CB(uint16_t u, BS_uint16_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Uint16(u);
	}
	bool Uint32(uint32_t& u) override
	{
		stream.Put(static_cast<char>(u >> 24));
		stream.Put(static_cast<char>(u >> 16));
		stream.Put(static_cast<char>(u >> 8));
		stream.Put(static_cast<char>(u));
		return stream.good();
	}
	bool Uint32_CB(uint32_t u, BS_uint32_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Uint32(u);
	}
	bool Uint64(uint64_t& u) override
	{
		stream.Put(static_cast<char>(u >> 56));
		stream.Put(static_cast<char>(u >> 48));
		stream.Put(static_cast<char>(u >> 40));
		stream.Put(static_cast<char>(u >> 32));
		stream.Put(static_cast<char>(u >> 24));
		stream.Put(static_cast<char>(u >> 16));
		stream.Put(static_cast<char>(u >> 8));
		stream.Put(static_cast<char>(u));
		return stream.good();
	}
	bool Uint64_CB(uint64_t u, BS_uint64_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Uint64(u);
	}
	bool Float(float& d) override
	{
		uint32_t fhold;
		memcpy(&fhold, &d, sizeof(fhold));
		return Uint32(fhold);
	}
	bool Float_CB(float d, BS_float_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Float(d);
	}
	bool Double(double& d) override
	{
		uint64_t fhold;
		memcpy(&fhold, &d, sizeof(fhold));
		return Uint64(fhold);
	}
	bool Double_CB(double d, BS_double_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		return Double(d);
	}
	bool String(std::string& str) override
	{
		return StringZ_CB(str, BS_MAX_STRING_SIZE, internal_read_string_cb, &str);
	}
	bool String_CB(std::string_view str, BS_string_cb cb, void* ud) override
	{
		return StringZ_CB(str, BS_MAX_STRING_SIZE, cb, ud);
	}
	bool StringZ(std::string& str, size_t max_size) override
	{
		return StringZ_CB(str, max_size, internal_read_string_cb, &str);
	}
	bool StringZ_CB(std::string_view str, size_t max_size, BS_string_cb cb, void* ud) override
	{
		(void)cb;
		(void)ud; // unused
		(void)max_size; // I should check this, but how would the error propogate?
		ASSERT(max_size <= BS_MAX_STRING_SIZE);
		ASSERT(str.size() <= max_size);
		uint16_t tmp = str.size();
		if(!Uint16(tmp))
		{
			return false;
		}
		for(char c : str)
		{
			stream.Put(c);
		}
		return stream.good();
	}
	bool Key(std::string_view str) override
	{
		(void)str; // unused
		return stream.good();
	}
	bool StartObject() override
	{
		return stream.good();
	}
	bool EndObject() override
	{
		return stream.good();
	}
	bool StartArray() override
	{
		return stream.good();
	}
	bool EndArray() override
	{
		return stream.good();
	}
};