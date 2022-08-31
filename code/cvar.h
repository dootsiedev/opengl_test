#pragma once

#include "global.h"
#include "RWops.h"

#include <cstring> //for strcmp

#include <map>
#include <string>

enum class CVAR_T
{
	// requires the app to be restarted
	STARTUP,
	// requires the game to be restarted (if there is a game)
	GAME,
	// you can modify this during runtime and changes should take effect
	RUNTIME,
	// warn when this setting is attempted to be set
	DISABLED,
	// the variable was never meant to be written
	READONLY
};

class V_cvar
{
public:
	const char* cvar_key;
	const char* cvar_comment;
	CVAR_T cvar_type;
	const char* cvar_debug_file;
	int cvar_debug_line;

	// this needs to be set manually on startup using cvar_write()
	// because the constructor gets called before the default value is set.
	std::string cvar_default_value;

	V_cvar(const char* key, const char* comment, CVAR_T type, const char* file, int line)
	: cvar_key(key)
	, cvar_comment(comment)
	, cvar_type(type)
	, cvar_debug_file(file)
	, cvar_debug_line(line)
	{
	}
	NDSERR virtual bool cvar_read(const char* buffer) = 0;
	virtual std::string cvar_write() = 0;
	virtual ~V_cvar() = default; // not used but needed to suppress warnings

	// no copy.
	V_cvar(const V_cvar&) = delete;
	V_cvar& operator=(const V_cvar&) = delete;
};

// const char* as the key because I feel like it.
struct cmp_str
{
	bool operator()(const char* a, const char* b) const
	{
		return strcmp(a, b) < 0;
	}
};

// std::map is perfect since it sorts all cvars for you.
std::map<const char*, V_cvar&, cmp_str>& get_convars();
void cvar_init(); // sets the default
// flags_req must be either CVAR_STARTUP,CVAR_GAME,CVAR_RUNTIME.
NDSERR bool cvar_args(CVAR_T flags_req, int argc, const char* const* argv);
// this will modify the string
NDSERR bool cvar_line(CVAR_T flags_req, char* line);
NDSERR bool cvar_file(CVAR_T flags_req, RWops* file);
void cvar_list(bool debug);

// to define an option for a single source file use this:
// static REGISTER_CVAR_INT(cv_name_of_option, 1, "this is an option", CVAR_NORMAL);
// you can use cv_name_of_option.data for reading and writing the value.
// to share a cvar in a header you can use:
// extern cvar_int cv_name_of_option;
// and then define REGISTER_CVAR_INT somewhere (without static).
#define REGISTER_CVAR_INT(key, value, comment, type) \
	cvar_int key(#key, value, comment, type, __FILE__, __LINE__)
#define REGISTER_CVAR_DOUBLE(key, value, comment, type) \
	cvar_double key(#key, value, comment, type, __FILE__, __LINE__)
#define REGISTER_CVAR_STRING(key, value, comment, type) \
	cvar_string key(#key, value, comment, type, __FILE__, __LINE__)

class cvar_int : public V_cvar
{
public:
	int data;

	// you should use REGISTER_CVAR_ to fill in file and line.
	cvar_int(
		const char* key, int value, const char* comment, CVAR_T type, const char* file, int line);

	NDSERR bool cvar_read(const char* buffer) override;
	std::string cvar_write() override;
};

class cvar_double : public V_cvar
{
public:
	double data;

	// you should use REGISTER_CVAR_ to fill in file and line.
	cvar_double(
		const char* key,
		double value,
		const char* comment,
		CVAR_T type,
		const char* file,
		int line);

	NDSERR bool cvar_read(const char* buffer) override;
	std::string cvar_write() override;
};

class cvar_string : public V_cvar
{
public:
	std::string data;

	// you should use REGISTER_CVAR_ to fill in file and line.
	cvar_string(
		const char* key,
		std::string value,
		const char* comment,
		CVAR_T type,
		const char* file,
		int line);

	NDSERR bool cvar_read(const char* buffer) override;
	std::string cvar_write() override;
};