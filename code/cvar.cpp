#include "global_pch.h"
#include "global.h"

#include <climits>

#include "cvar.h"

// for reading files, since I like the stream API.
#include "BS_Archive/BS_stream.h"

// unfortunately std::from_chars for doubles doesn't have great support on compilers...
// so you need a pretty decent version of gcc or clang...
// I want this because I can use non-null terminating strings for cvar_read....
//#include <charconv>

// TODO: make each cvar type have a name, so when you get an error or whatever, you can see the type
// it expects.
// TODO: I can still modify startup cvar values, because of "soft reboot",
// but I should store the value and load it later because it can sometimes cause bugs.

std::map<const char*, V_cvar&, cmp_str>& get_convars()
{
	static std::map<const char*, V_cvar&, cmp_str> convars;
	return convars;
}

cvar_int::cvar_int(
	const char* key, int value, const char* comment, CVAR_T type, const char* file, int line)
: V_cvar(key, comment, type, file, line)
, data(value)
{
	auto [it, success] = get_convars().try_emplace(key, *this);
	(void)success;
	// this shouldn't be possible.
	ASSERT(success && "cvar already registered");
}
bool cvar_int::cvar_read(const char* buffer)
{
	char* end_ptr;

	pop_errno_t pop_errno;

	// unfortunatly there is no strtoi, and longs could be 4 or 8 bytes...
	// NOLINTNEXTLINE(google-runtime-int)
	long value = strtol(buffer, &end_ptr, 10);

	const int cmax = std::numeric_limits<int>::max();
	const int cmin = std::numeric_limits<int>::min();

	if(errno == ERANGE)
	{
		serrf("+%s: cvar value out of range: \"%s\"\n", cvar_key, buffer);
		return false;
	}
	if(value > cmax || value < cmin)
	{
		serrf(
			"+%s: cvar value out of range: \"%s\" min: %d, max: %d, result: %ld\n",
			cvar_key,
			buffer,
			cmin,
			cmax,
			value);
		return false;
	}
	if(end_ptr == buffer)
	{
		serrf("+%s: cvar value not valid numeric input: \"%s\"\n", cvar_key, buffer);
		return false;
	}

	if(*end_ptr != '\0')
	{
		slogf("+%s: warning cvar value extra characters on input: \"%s\"\n", cvar_key, buffer);
	}

	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	data = value;

	return true;
}
std::string cvar_int::cvar_write()
{
	std::ostringstream oss;
	oss << data;
	return oss.str();
}

cvar_double::cvar_double(
	const char* key, double value, const char* comment, CVAR_T type, const char* file, int line)
: V_cvar(key, comment, type, file, line)
, data(value)
{
	auto [it, success] = get_convars().try_emplace(key, *this);
	(void)success;
	// this shouldn't be possible.
	ASSERT(success && "cvar already registered");
}
bool cvar_double::cvar_read(const char* buffer)
{
	char* end_ptr;

	pop_errno_t pop_errno;
	double value = strtod(buffer, &end_ptr);
	if(errno == ERANGE)
	{
		serrf("+%s: cvar value out of range: \"%s\"\n", cvar_key, buffer);
		return false;
	}
	if(end_ptr == buffer)
	{
		serrf("+%s: cvar value not valid numeric input: \"%s\"\n", cvar_key, buffer);
		return false;
	}

	if(*end_ptr != '\0')
	{
		slogf("+%s: warning cvar value extra characters on input: \"%s\"\n", cvar_key, buffer);
	}

	data = value;
	// slogf("%s = %f\n", cvar_key, data);

	return true;
}
std::string cvar_double::cvar_write()
{
	std::ostringstream oss;
	oss << data;
	return oss.str();
}

cvar_string::cvar_string(
	const char* key,
	std::string value,
	const char* comment,
	CVAR_T type,
	const char* file,
	int line)
: V_cvar(key, comment, type, file, line)
, data(std::move(value))
{
	auto [it, success] = get_convars().try_emplace(key, *this);
	(void)success;
	// this shouldn't be possible.
	ASSERT(success && "cvar already registered");
}
bool cvar_string::cvar_read(const char* buffer)
{
	ASSERT(buffer);
	data = buffer;
	return true;
}
std::string cvar_string::cvar_write()
{
	return data;
}

void cvar_init()
{
	for(const auto& it : get_convars())
	{
		it.second.cvar_default_value = it.second.cvar_write();
	}
}

static void print_cvar(V_cvar& cvar, bool debug = false)
{
	std::string value = cvar.cvar_write();
	const char* type = NULL;
	switch(cvar.cvar_type)
	{
	case CVAR_T::RUNTIME: type = ""; break;
	case CVAR_T::STARTUP: type = " [STARTUP]"; break;
	case CVAR_T::DEFFERRED: type = " [DEFFERRED]"; break;
	case CVAR_T::READONLY: type = " [READONLY]"; break;
	case CVAR_T::DISABLED: type = " [DISABLED]"; break;
	default: ASSERT("unreachable" && false);
	}
	slogf("%s%s: \"%s\"\n", cvar.cvar_key, type, value.c_str());
	if(debug)
	{
		slogf("\tFile: %s\n", cvar.cvar_debug_file);
		slogf("\tLine: %d\n", cvar.cvar_debug_line);
	}
	slogf("\t%s\n", cvar.cvar_comment);
}

int cvar_arg(CVAR_T flags_req, int argc, const char* const* argv)
{
	int i = 0;
	for(; i < argc; ++i)
	{
		if(argv[i][0] != '+')
		{
			auto it = get_convars().find(argv[i]);
			if(it == get_convars().end())
			{
				serrf(
					"ERROR: cvar option must start with a '+'\n"
					"expression: `%s`\n",
					argv[i]);
				return -1;
			}
			// print the value.
			// slogf("%s: %s\n", argv[i], it->second.cvar_write().c_str());
			print_cvar(it->second);
			continue;
		}

		const char* name = argv[i] + 1;
		auto it = get_convars().find(name);
		if(it == get_convars().end())
		{
			serrf("ERROR: cvar not found: `%s`\n", name);
			return -1;
		}

		V_cvar& cv = it->second;
		bool ignore = false;

		// go to next argument.
		i++;
		if(i >= argc)
		{
			serrf("ERROR: cvar assignment missing: `%s`\n", name);
			return -1;
		}

		if(cv.cvar_type == CVAR_T::DISABLED)
		{
			slogf("warning: cvar disabled: `%s`\n", name);
			ignore = true;
		}

		switch(flags_req)
		{
		case CVAR_T::RUNTIME:
			if(cv.cvar_type == CVAR_T::DEFFERRED)
			{
				slogf("info: cvar defferred: `%s`\n", name);
				break;
			}
			[[fallthrough]];
		case CVAR_T::DEFFERRED:
			if(cv.cvar_type == CVAR_T::STARTUP)
			{
				slogf("warning: cvar must be set on startup to take effect: `%s`\n", name);
				// TODO: some way to write the cvar into a startup file?
				// maybe even a way to completely restart without forcing an exit?
				// ignore = true;
				break;
			}
			[[fallthrough]];
		case CVAR_T::STARTUP: break;
		default: ASSERT(false && "flags_req not implemented");
		}

		if(!ignore)
		{
			std::string old_value = cv.cvar_write();
			if(!cv.cvar_read(argv[i]))
			{
				return -1;
			}
			slogf("%s (%s) = %s\n", name, old_value.c_str(), argv[i]);
		}
	}
	return i;
}

// this IS stupid, but a portable strtok is complicated.
// probably should put this into global.h if I used it more.
static char* musl_strtok_r(char* __restrict s, const char* __restrict sep, char** __restrict p)
{
	// NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if(!s && !(s = *p)) return NULL;
	s += strspn(s, sep);
	// NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if(!*s) return *p = 0;
	*p = s + strcspn(s, sep);
	// NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if(**p)
		*(*p)++ = 0;
	else
		*p = 0;
	return s;
}

bool cvar_line(CVAR_T flags_req, char* line)
{
	// TODO (dootsie): could try to support escape keys since I can't insert quotes or newlines?
	std::vector<const char*> arguments;
	char* token = line;
	bool in_quotes = false;
	while(token != NULL)
	{
		char* next_quote = strchr(token, '\"');
		if(next_quote != NULL)
		{
			*next_quote++ = '\0';
			// note the in_quotes condition is the opposite
			// because I toggle before the condition.
			in_quotes = !in_quotes;
			if(!in_quotes)
			{
				arguments.push_back(token);
				token = next_quote;
				continue;
			}
		}

		const char* delim = " ";
		char* next_token = NULL;
		token = musl_strtok_r(token, delim, &next_token);
		while(token != NULL)
		{
			arguments.push_back(token);
			token = musl_strtok_r(NULL, delim, &next_token);
		}

		token = next_quote;
	}

	if(in_quotes)
	{
		serrf("missing quote pair\n");
		return false;
	}

	const char** argv = arguments.data();
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	int argc = arguments.size();
	while(argc > 0)
	{
		int ret = cvar_arg(flags_req, argc, argv);
		if(ret == -1)
		{
			return false;
		}
		argc -= ret;
		argv += ret;
	}

	return true;
}

bool cvar_file(CVAR_T flags_req, RWops* file)
{
	ASSERT(file != NULL);
	char buffer[1000];
	BS_ReadStream reader(file, buffer, sizeof(buffer));

	char line_buf[1000 + 1];

	// size_t count = 0;
	char* pos = line_buf;
	char* end = line_buf + sizeof(line_buf);
	while(pos < end)
	{
		*pos = reader.Take();
		//++count;
		if(*pos == '\n')
		{
			if(line_buf[0] == '#' || pos == line_buf)
			{
				// comment or empty, ignore.
				// ATM empty lines do not cause an error in cvar_line.
				// but that might change.
			}
			else
			{
				*pos = '\0';
				// slogf("%s\n", line_buf);
				if(!cvar_line(flags_req, line_buf))
				{
					return false;
				}
			}
			pos = line_buf;
		}
		else if(*pos == '\r')
		{
			// don't parse this.
			// windows will insert it for newlines.
		}
		else if(*pos == '\0')
		{
			if(!cvar_line(flags_req, line_buf))
			{
				return false;
			}
			break;
		}
		else
		{
			++pos;
		}
	}
	if(pos == end)
	{
		size_t max_line_size = sizeof(line_buf) - 1;
		serrf("line too long: %s (max: %zu)\n", file->name(), max_line_size);
		return false;
	}
	return true;
}

void cvar_list(bool debug)
{
	slog("cvar types:\n"
		 "-RUNTIME:\t"
		 "normal, changes should take effect\n"
		 "-[STARTUP]:\t"
		 "requires the app to be restarted\n"
		 "-[DEFFERRED]:\t"
		 "the value is cached in some way, so the value may not make instant changes\n"
		 "-[READONLY]:\t"
		 "the value cannot be set\n"
		 "-[DISABLED]\t"
		 "the value cannot be read or set due to platform or build options\n");
	for(const auto& it : get_convars())
	{
		print_cvar(it.second, debug);
	}
}
