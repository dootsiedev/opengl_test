#include "global.h"

#include "BS_Archive/BS_json.h"
#include "BS_Archive/BS_stream.h"

#include <climits>

#include "cvar.h"

// unfortunately std::from_chars for doubles doesn't have great support on compilers...
// so you need a pretty decent version of gcc or clang...
// I want this because I can use non-null terminating strings for cvar_read....
//#include <charconv>

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
		serrf("Error: cvar value out of range: \"+%s %s\"\n", cvar_key, buffer);
		return false;
	}
    if(value > cmax || value < cmin)
	{
		serrf(
			"Error: cvar value out of range: \"+%s %s\" min: %d, max: %d, result: %ld\n",
			cvar_key,
			buffer,
			cmin,
			cmax,
			value);
		return false;
	}
	if(end_ptr == buffer)
	{
		serrf("Error: cvar value not valid numeric input: \"+%s %s\"\n", cvar_key, buffer);
		return false;
	}

	if(*end_ptr != '\0')
	{
		slogf("warning: cvar value extra characters on input: \"+%s %s\"\n", cvar_key, buffer);
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
		serrf("Error: cvar value out of range: \"+%s %s\"\n", cvar_key, buffer);
		return false;
	}
	if(end_ptr == buffer)
	{
		serrf("Error: cvar value not valid numeric input: \"+%s %s\"\n", cvar_key, buffer);
		return false;
	}

	if(*end_ptr != '\0')
	{
		slogf("warning: cvar value extra characters on input: \"+%s %s\"\n", cvar_key, buffer);
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
	const char* key, std::string value, const char* comment, CVAR_T type, const char* file, int line)
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

bool cvar_args(CVAR_T flags_req, int argc, const char* const* argv)
{
	for(int i = 0; i < argc; ++i)
	{
		if(argv[i][0] != '+')
		{
			serrf(
				"ERROR: cvar option must start with a '+'\n"
				"expression: `%s`\n",
				argv[i]);
			return false;
		}

		const char* name = argv[i] + 1;
		auto it = get_convars().find(name);
		if(it == get_convars().end())
		{
			serrf("ERROR: cvar not found: `%s`\n", name);
			return false;
		}

		V_cvar& cv = it->second;

		// go to next argument.
		i++;
		if(i >= argc)
		{
			serrf("ERROR: cvar assignment missing: `%s`\n", name);
			return false;
		}

		if(cv.cvar_type == CVAR_T::DISABLED)
		{
			slogf("warning: cvar disabled: `%s`\n", name);
			continue;
		}

		switch(flags_req)
		{
		case CVAR_T::RUNTIME:
			if(cv.cvar_type == CVAR_T::GAME)
			{
				slogf("warning: cvar must be set pre-game take effect: `%s`\n", name);
				continue;
			}
			[[fallthrough]];
		case CVAR_T::GAME:
			if(cv.cvar_type == CVAR_T::STARTUP)
			{
				slogf("warning: cvar must be set on startup to take effect: `%s`\n", name);
				continue;
			}
			[[fallthrough]];
		case CVAR_T::STARTUP: break;
		default: ASSERT(false && "flags_req not implemented");
		}

        std::string old_value = cv.cvar_write();

		if(!cv.cvar_read(argv[i]))
		{
			return false;
		}
		slogf("%s (%s) = %s\n", name, old_value.c_str(), argv[i]);
	}
	return true;
}


bool cvar_json(RWops* file)
{
    // TODO: this SUCKS, just use a txt file ifstream + getline(), and parse the line like a console
    TIMER_U tick1;
	TIMER_U tick2;
	tick1 = timer_now();

	ASSERT(file != NULL);
	char buffer[1000];
	BS_ReadStream sb(file, buffer, sizeof(buffer));
	BS_JsonReader<decltype(sb), rj::kParseCommentsFlag | rj::kParseNumbersAsStringsFlag> ar(sb);

	std::string json_key;
	ar.StartObject();
	while(ar.Good())
	{
        auto key_cb = [](const char* str, size_t size, void* ud) {
            (void)size;
            if(strncmp(str, "END", size) == 0)
            {
                return true;
            }
			auto *it = static_cast<decltype(get_convars().end())*>(ud);
			*it = get_convars().find(str);
			if(*it == get_convars().end())
			{
				serrf("ERROR: cvar not found: `%s`\n", str);
				return false;
			}
			return true;
		};
        auto temp_it = get_convars().end();

		// This is actually sketchy because this should be a "Key"
        // but rapidjson just casts Keys to Strings so this works.
        if(!ar.String_CB(json_key, key_cb, &temp_it))
        {
            break;
        }
        if(temp_it == get_convars().end())
        {
            ar.Null();
            break;
        }

		auto value_cb = [](const char* str, size_t size, void* ud) {
            (void)size;
			auto *it = static_cast<decltype(get_convars().end())*>(ud);
			V_cvar& cv = (*it)->second;
			std::string old_value = cv.cvar_write();
			if(!cv.cvar_read(str))
			{
				return false;
			}
			slogf("%s (%s) = %s\n", (*it)->first, old_value.c_str(), str);

			return true;
		};
		if(!ar.String_CB(std::string(), value_cb, &temp_it))
		{
			break;
		}
	}
	ar.EndObject();

	if(!ar.Finish(file->name()))
    {
        return false;
    }
	tick2 = timer_now();
    slogf("%s time: %f\n", __func__, timer_delta_ms(tick1, tick2));
    return true;
}

void cvar_list(bool debug)
{
	slog("cvar types:\n"
		 "-CVAR_RUNTIME:\t"
		 "normal, changes should take effect\n"
		 "-[S] CVAR_STARTUP:\t"
		 "requires the app to be restarted\n"
		 "-[G] CVAR_GAME:\t"
		 "requires the game to be restarted (if there is a game)\n"
		 "-[R] CVAR_READONLY:\t"
		 "the value cannot be set\n"
		 "-[D] CVAR_DISABLED\t"
		 "the value cannot be read or set\n");
	for(const auto& it : get_convars())
	{
		std::string value = it.second.cvar_write();
		const char* type = NULL;
		switch(it.second.cvar_type)
		{
		case CVAR_T::RUNTIME: type = ""; break;
		case CVAR_T::STARTUP: type = "[S]"; break;
		case CVAR_T::GAME: type = "[G]"; break;
		case CVAR_T::READONLY: type = "[R]"; break;
		case CVAR_T::DISABLED: type = "[D]"; break;
		default: ASSERT("unreachable" && false);
		}
		slogf("%s %s: \"%s\"\n", it.second.cvar_key, type, value.c_str());
		if(debug)
		{
			slogf("\tFile: %s\n", it.second.cvar_debug_file);
			slogf("\tLine: %d\n", it.second.cvar_debug_line);
		}
		slogf("\t%s\n", it.second.cvar_comment);
	}
}
