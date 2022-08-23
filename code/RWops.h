#pragma once

#include <inttypes.h>
#include <stdio.h>

#include <memory>

typedef long RW_ssize_t; // NOLINT

class RWops
{
public:
	//follows the same error specifications as stdio functions.
	//instead of ferror() use serr_check_error()

    //returns the name of the stream.
	virtual const char* name() = 0;
	virtual size_t read(void *ptr, size_t size, size_t nmemb) = 0;
	virtual size_t write(const void *ptr, size_t size, size_t nmemb) = 0;
	virtual int seek(RW_ssize_t offset, int whence) = 0;
	virtual RW_ssize_t tell() = 0;
	virtual RW_ssize_t size() = 0;
    //a segfault will occur if you call close() or other functions afterwards.
	virtual bool close() = 0;
	//the one annoying quirk is that the destructor won't return an error, so consider using close()
	//so you should only trigger the destructor if there is already an error because this could leak serr.
	virtual ~RWops() = default;
};

typedef std::unique_ptr<RWops> Unique_RWops;

//will print an error so that you can pass it into RWops_Stdio
FILE* serr_wrapper_fopen(const char* path, const char* mode);

class RWops_Stdio : public RWops
{
public:
    //I could replace stream_info with fstat filename, but I like customization.
	std::string stream_name;
    //the fp will be closed with fclose(), and size() will use fstat.
    FILE* fp;
    RWops_Stdio(FILE* stream, std::string file);

	const char* name() override;
	size_t read(void *ptr, size_t size, size_t nmemb) override;
	size_t write(const void *ptr, size_t size, size_t nmemb) override;
	int seek(RW_ssize_t offset, int whence) override;
	RW_ssize_t tell() override;
	RW_ssize_t size() override;
    bool close() override;
	~RWops_Stdio() override;
};


Unique_RWops Unique_RWops_OpenFS(std::string path, const char* mode);
Unique_RWops Unique_RWops_FromFP(FILE* fp, std::string name = std::string());

//this will not allocate during writing
Unique_RWops Unique_RWops_FromMemory(char* memory, size_t size, bool readonly = false, std::string name = std::string());