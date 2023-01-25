#include "global_pch.h"
#include "global.h"
#include "keybind.h"

// TODO: would be cool if I could make a custom keybind system
// where a key will trigger a basic cvar command string
// but the reason why keybind defintions are cvar strings instead of having a quake style bind
// system is because I plan on adding a analog keybind (another cvar type) for game controllers (but
// also supports keyboard).
// Actually ignore the above, I wouldn't add in a analog keybind cvar,
// just a string with the controller mappings, and a bool that enables controller support.
// Maybe what I need is a way to make cvars dependant on other cvars, so if this cvar is ==1,
// disable this cvar. but this is tricky to handle because of static initialization order, I think.

// TODO: I think allowing 2 keys for the same input would be useful, since I use ctrl and c for
// crouching...

#define KEYBIND_NONE_STRING_NAME "NONE"
#define MOUSE_LMB_STRING_NAME "LMB"
#define MOUSE_RMB_STRING_NAME "RMB"
#define MOUSE_MMB_STRING_NAME "MMB"

// needs a mask because apparently scroll lock is included....
#define MY_ALLOWED_KMODS (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT)

// forward declaration
static Uint16 find_sdl_mod(const char* string, size_t size);
static const char* get_sdl_mod_name(Uint16 mod);
static SDL_Keycode find_sdl_keycode(const char* string, size_t size);
static const char* get_sdl_key_name(SDL_Keycode key);

cvar_key_bind::cvar_key_bind(
	const char* key,
	keybind_state value,
	bool allow_mouse_,
	const char* comment,
	CVAR_T type,
	const char* file,
	int line)
: V_cvar(key, comment, type, file, line)
, allow_mouse(allow_mouse_)
{
	{
		auto [it, success] = get_convars().try_emplace(key, *this);
		(void)success;
		// this shouldn't be possible.
		ASSERT(success && "cvar already registered");
	}
	key_binds = value;
}
bool cvar_key_bind::cvar_read(const char* buffer)
{
	ASSERT(buffer);
	// reset values.
	key_binds.type = KEYBIND_T::NONE;
	const char* token_start = buffer;
	const char* token_cur = buffer;
	for(; *token_cur != '\0'; ++token_cur)
	{
		if(*token_cur == ';' && token_cur != token_start)
		{
			// can't use SDL_GetKeyName because null terminator,
			// but I think the performance is about the same.
			if(!convert_string_to_event(token_start, token_cur - token_start))
			{
				return false;
			}
			token_start = token_cur + 1;
			continue;
		}
	}
	if(token_cur != token_start)
	{
		if(!convert_string_to_event(token_start, token_cur - token_start))
		{
			return false;
		}
	}
	return true;
}
std::string cvar_key_bind::keybind_to_string(keybind_state& in)
{
	std::string out;

	if(in.mod != 0)
	{
		const char* mod_name = get_sdl_mod_name(in.mod);
		if(mod_name != NULL)
		{
			out += mod_name;
			out += ';';
		}
		else
		{
			// probably should be an serr error, same for get_sdl_key_name.
			out += "UNKNOWN_MODIFIER;";
		}
	}
	switch(in.type)
	{
	case KEYBIND_T::NONE: out += KEYBIND_NONE_STRING_NAME; break;
	case KEYBIND_T::KEY: out += get_sdl_key_name(in.key); break;
	case KEYBIND_T::MOUSE:
		switch(in.mouse_button)
		{
		case SDL_BUTTON_LEFT: out += MOUSE_LMB_STRING_NAME; break;
		case SDL_BUTTON_RIGHT: out += MOUSE_RMB_STRING_NAME; break;
		case SDL_BUTTON_MIDDLE: out += MOUSE_MMB_STRING_NAME; break;
		default: out += "UNKNOWN_MOUSE_BUTTON????";
		}
		break;
	}

	return out;
}

std::string cvar_key_bind::cvar_write()
{
	return keybind_to_string(key_binds);
}

bool cvar_key_bind::bind_sdl_event(keybind_state& value, SDL_Event& e)
{
	switch(e.type)
	{
		// I use button up because if I try to use the modifier,
		// the modifier would be eaten as the bind before you could press another button.
	case SDL_KEYUP:
		value.type = KEYBIND_T::KEY;
		value.key = e.key.keysym.sym;
		// The only problem with this is that when I lift the modifier, it would overwrite the
		// bind... workaround: just hold the modifier and click confirmation...
		value.mod = e.key.keysym.mod & MY_ALLOWED_KMODS;
		return true;
	case SDL_MOUSEBUTTONDOWN:
		if(allow_mouse)
		{
			value.type = KEYBIND_T::MOUSE;
			value.mouse_button = e.button.button;
			value.mod = SDL_GetModState() & MY_ALLOWED_KMODS;
			return true;
		}
	}
	return false;
}
keybind_compare_type cvar_key_bind::compare_sdl_event(SDL_Event& e, keybind_compare_type flags)
{
	keybind_compare_type mask = 0;
	switch(key_binds.type)
	{
	case KEYBIND_T::NONE: return KEYBIND_NULL;
	case KEYBIND_T::KEY:
		switch(e.type)
		{
		case SDL_KEYDOWN:
			// this doesn't apply to keyup, because then we will never get a keyup for wasd!
			// I know that the correct way of dealing with this is by polling the key state,
			// and replace mod with a keycode, but I don't want to do that because I am lazy.
			// and this only affects "held" keys like movement,
			// I don't have any keyup oneshot buttons (ATM), and if I did why would I need mod?.
			if(key_binds.mod != 0 && (key_binds.mod & e.key.keysym.mod) == 0)
			{
				// modifier required.
				break;
			}
			[[fallthrough]];
		case SDL_KEYUP:
			if(e.key.keysym.sym == key_binds.key)
			{
				if(e.key.repeat != 0)
				{
					if((flags & KEYBIND_OR_REPEAT) != 0)
					{
						mask |= KEYBIND_OR_REPEAT;
					}
				}
				if(e.key.state == SDL_PRESSED && (flags & KEYBIND_BUTTON_DOWN) != 0)
				{
					mask |= KEYBIND_BUTTON_DOWN;
				}
				else if(e.key.state == SDL_RELEASED && (flags & KEYBIND_BUTTON_UP) != 0)
				{
					mask |= KEYBIND_BUTTON_UP;
				}
				return mask;
			}
			break;
		}
		break;
	case KEYBIND_T::MOUSE:
		switch(e.type)
		{
		case SDL_MOUSEBUTTONDOWN:
			if(key_binds.mod != 0 && (key_binds.mod & e.key.keysym.mod) == 0)
			{
				// modifier required.
				break;
			}
			[[fallthrough]];
		case SDL_MOUSEBUTTONUP:
			if(e.button.button == key_binds.mouse_button)
			{
				if(e.button.state == SDL_PRESSED && (flags & KEYBIND_BUTTON_DOWN) != 0)
				{
					mask |= KEYBIND_BUTTON_DOWN;
				}
				else if(e.button.state == SDL_RELEASED && (flags & KEYBIND_BUTTON_UP) != 0)
				{
					mask |= KEYBIND_BUTTON_UP;
				}
				return mask;
			}
			break;
		}
		break;
	}

	return KEYBIND_NULL;
}

bool cvar_key_bind::convert_string_to_event(const char* buffer, size_t size)
{
	if(strlen(KEYBIND_NONE_STRING_NAME) == size &&
	   strncmp(KEYBIND_NONE_STRING_NAME, buffer, size) == 0)
	{
		key_binds.type = KEYBIND_T::NONE;
		return true;
	}
	SDL_Keycode key = find_sdl_keycode(buffer, size);
	if(key != -1)
	{
		key_binds.type = KEYBIND_T::KEY;
		key_binds.key = key;
		return true;
	}
	Uint16 mod = find_sdl_mod(buffer, size);
	if(mod != 0)
	{
		// this isn't a key, just modify the "next slot"
		// for the next string that isn't a modifier
		key_binds.mod |= mod;
		return true;
	}
	if(strlen(MOUSE_LMB_STRING_NAME) == size && strncmp(MOUSE_LMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds.type = KEYBIND_T::MOUSE;
		key_binds.mouse_button = SDL_BUTTON_LEFT;
		return true;
	}
	if(strlen(MOUSE_RMB_STRING_NAME) == size && strncmp(MOUSE_RMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds.type = KEYBIND_T::MOUSE;
		key_binds.mouse_button = SDL_BUTTON_RIGHT;
		return true;
	}
	if(strlen(MOUSE_MMB_STRING_NAME) == size && strncmp(MOUSE_MMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds.type = KEYBIND_T::MOUSE;
		key_binds.mouse_button = SDL_BUTTON_MIDDLE;
		return true;
	}

	// flags
	serrf("unknown key bind string: %*.s\n", static_cast<int>(size), buffer);
	return false;
}

#define MY_MOD_MAP(XX) \
	XX(KMOD_LSHIFT)    \
	XX(KMOD_RSHIFT)    \
	XX(KMOD_LCTRL)     \
	XX(KMOD_RCTRL)     \
	XX(KMOD_LALT)      \
	XX(KMOD_RALT)      \
	XX(KMOD_CTRL)      \
	XX(KMOD_SHIFT)     \
	XX(KMOD_ALT)

// returns = 0 for no modifier
Uint16 find_sdl_mod(const char* string, size_t size)
{
#define XX(code)                                                   \
	if(strlen(#code) == size && strncmp(string, #code, size) == 0) \
	{                                                              \
		return code;                                               \
	}
	MY_MOD_MAP(XX)
#undef XX
	return 0;
}

const char* get_sdl_mod_name(Uint16 mod)
{
#define XX(code)      \
	if((code) == mod) \
	{                 \
		return #code; \
	}
	MY_MOD_MAP(XX)
#undef XX
	return NULL;
}

#define MY_KEYCODE_MAP(XX) \
	XX(RETURN)             \
	XX(ESCAPE)             \
	XX(BACKSPACE)          \
	XX(TAB)                \
	XX(SPACE)              \
	XX(EXCLAIM)            \
	XX(QUOTEDBL)           \
	XX(HASH)               \
	XX(PERCENT)            \
	XX(DOLLAR)             \
	XX(AMPERSAND)          \
	XX(QUOTE)              \
	XX(LEFTPAREN)          \
	XX(RIGHTPAREN)         \
	XX(ASTERISK)           \
	XX(PLUS)               \
	XX(COMMA)              \
	XX(MINUS)              \
	XX(PERIOD)             \
	XX(SLASH)              \
	XX(0)                  \
	XX(1)                  \
	XX(2)                  \
	XX(3)                  \
	XX(4)                  \
	XX(5)                  \
	XX(6)                  \
	XX(7)                  \
	XX(8)                  \
	XX(9)                  \
	XX(COLON)              \
	XX(SEMICOLON)          \
	XX(LESS)               \
	XX(EQUALS)             \
	XX(GREATER)            \
	XX(QUESTION)           \
	XX(AT)                 \
	XX(LEFTBRACKET)        \
	XX(BACKSLASH)          \
	XX(RIGHTBRACKET)       \
	XX(CARET)              \
	XX(UNDERSCORE)         \
	XX(BACKQUOTE)          \
	XX(a)                  \
	XX(b)                  \
	XX(c)                  \
	XX(d)                  \
	XX(e)                  \
	XX(f)                  \
	XX(g)                  \
	XX(h)                  \
	XX(i)                  \
	XX(j)                  \
	XX(k)                  \
	XX(l)                  \
	XX(m)                  \
	XX(n)                  \
	XX(o)                  \
	XX(p)                  \
	XX(q)                  \
	XX(r)                  \
	XX(s)                  \
	XX(t)                  \
	XX(u)                  \
	XX(v)                  \
	XX(w)                  \
	XX(x)                  \
	XX(y)                  \
	XX(z)                  \
	XX(CAPSLOCK)           \
	XX(F1)                 \
	XX(F2)                 \
	XX(F3)                 \
	XX(F4)                 \
	XX(F5)                 \
	XX(F6)                 \
	XX(F7)                 \
	XX(F8)                 \
	XX(F9)                 \
	XX(F10)                \
	XX(F11)                \
	XX(F12)                \
	XX(PRINTSCREEN)        \
	XX(SCROLLLOCK)         \
	XX(PAUSE)              \
	XX(INSERT)             \
	XX(HOME)               \
	XX(PAGEUP)             \
	XX(DELETE)             \
	XX(END)                \
	XX(PAGEDOWN)           \
	XX(RIGHT)              \
	XX(LEFT)               \
	XX(DOWN)               \
	XX(UP)                 \
	XX(NUMLOCKCLEAR)       \
	XX(KP_DIVIDE)          \
	XX(KP_MULTIPLY)        \
	XX(KP_MINUS)           \
	XX(KP_PLUS)            \
	XX(KP_ENTER)           \
	XX(KP_1)               \
	XX(KP_2)               \
	XX(KP_3)               \
	XX(KP_4)               \
	XX(KP_5)               \
	XX(KP_6)               \
	XX(KP_7)               \
	XX(KP_8)               \
	XX(KP_9)               \
	XX(KP_0)               \
	XX(KP_PERIOD)          \
	XX(APPLICATION)        \
	XX(POWER)              \
	XX(KP_EQUALS)          \
	XX(F13)                \
	XX(F14)                \
	XX(F15)                \
	XX(F16)                \
	XX(F17)                \
	XX(F18)                \
	XX(F19)                \
	XX(F20)                \
	XX(F21)                \
	XX(F22)                \
	XX(F23)                \
	XX(F24)                \
	XX(EXECUTE)            \
	XX(HELP)               \
	XX(MENU)               \
	XX(SELECT)             \
	XX(STOP)               \
	XX(AGAIN)              \
	XX(UNDO)               \
	XX(CUT)                \
	XX(COPY)               \
	XX(PASTE)              \
	XX(FIND)               \
	XX(MUTE)               \
	XX(VOLUMEUP)           \
	XX(VOLUMEDOWN)         \
	XX(KP_COMMA)           \
	XX(KP_EQUALSAS400)     \
	XX(ALTERASE)           \
	XX(SYSREQ)             \
	XX(CANCEL)             \
	XX(CLEAR)              \
	XX(PRIOR)              \
	XX(RETURN2)            \
	XX(SEPARATOR)          \
	XX(OUT)                \
	XX(OPER)               \
	XX(CLEARAGAIN)         \
	XX(CRSEL)              \
	XX(EXSEL)              \
	XX(KP_00)              \
	XX(KP_000)             \
	XX(THOUSANDSSEPARATOR) \
	XX(DECIMALSEPARATOR)   \
	XX(CURRENCYUNIT)       \
	XX(CURRENCYSUBUNIT)    \
	XX(KP_LEFTPAREN)       \
	XX(KP_RIGHTPAREN)      \
	XX(KP_LEFTBRACE)       \
	XX(KP_RIGHTBRACE)      \
	XX(KP_TAB)             \
	XX(KP_BACKSPACE)       \
	XX(KP_A)               \
	XX(KP_B)               \
	XX(KP_C)               \
	XX(KP_D)               \
	XX(KP_E)               \
	XX(KP_F)               \
	XX(KP_XOR)             \
	XX(KP_POWER)           \
	XX(KP_PERCENT)         \
	XX(KP_LESS)            \
	XX(KP_GREATER)         \
	XX(KP_AMPERSAND)       \
	XX(KP_DBLAMPERSAND)    \
	XX(KP_VERTICALBAR)     \
	XX(KP_DBLVERTICALBAR)  \
	XX(KP_COLON)           \
	XX(KP_HASH)            \
	XX(KP_SPACE)           \
	XX(KP_AT)              \
	XX(KP_EXCLAM)          \
	XX(KP_MEMSTORE)        \
	XX(KP_MEMRECALL)       \
	XX(KP_MEMCLEAR)        \
	XX(KP_MEMADD)          \
	XX(KP_MEMSUBTRACT)     \
	XX(KP_MEMMULTIPLY)     \
	XX(KP_MEMDIVIDE)       \
	XX(KP_PLUSMINUS)       \
	XX(KP_CLEAR)           \
	XX(KP_CLEARENTRY)      \
	XX(KP_BINARY)          \
	XX(KP_OCTAL)           \
	XX(KP_DECIMAL)         \
	XX(KP_HEXADECIMAL)     \
	XX(LCTRL)              \
	XX(LSHIFT)             \
	XX(LALT)               \
	XX(LGUI)               \
	XX(RCTRL)              \
	XX(RSHIFT)             \
	XX(RALT)               \
	XX(RGUI)               \
	XX(MODE)               \
	XX(AUDIONEXT)          \
	XX(AUDIOPREV)          \
	XX(AUDIOSTOP)          \
	XX(AUDIOPLAY)          \
	XX(AUDIOMUTE)          \
	XX(MEDIASELECT)        \
	XX(WWW)                \
	XX(MAIL)               \
	XX(CALCULATOR)         \
	XX(COMPUTER)           \
	XX(AC_SEARCH)          \
	XX(AC_HOME)            \
	XX(AC_BACK)            \
	XX(AC_FORWARD)         \
	XX(AC_STOP)            \
	XX(AC_REFRESH)         \
	XX(AC_BOOKMARKS)       \
	XX(BRIGHTNESSDOWN)     \
	XX(BRIGHTNESSUP)       \
	XX(DISPLAYSWITCH)      \
	XX(KBDILLUMTOGGLE)     \
	XX(KBDILLUMDOWN)       \
	XX(KBDILLUMUP)         \
	XX(EJECT)              \
	XX(SLEEP)              \
	XX(APP1)               \
	XX(APP2)               \
	XX(AUDIOREWIND)        \
	XX(AUDIOFASTFORWARD)

// this is horrible, but because I need SDL_GetKeyName and I haven't initialized SDL
// also technically I might stop using SDL but I would still use SDL's names.

// NOLINTNEXTLINE
SDL_Keycode find_sdl_keycode(const char* string, size_t size)
{
#define XX(code)                                                   \
	if(strlen(#code) == size && strncmp(string, #code, size) == 0) \
	{                                                              \
		return SDLK_##code;                                        \
	}
	MY_KEYCODE_MAP(XX)
	return -1;
#undef XX
}

// I know I don't need to copy-paste this if I used the X macro
// but I didn't expect SDL_GetKeyName to not return SDLK_ formatted strings!!!!
// NOLINTNEXTLINE
const char* get_sdl_key_name(SDL_Keycode key)
{
#define XX(code) \
	case(SDLK_##code): return #code;
	switch(key)
	{
		MY_KEYCODE_MAP(XX)
	}
#undef XX
	return "UNKNOWN_KEY_CODE????";
}