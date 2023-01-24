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

#define MY_KEYCODE_MAP(XX)      \
	XX(SDLK_RETURN)             \
	XX(SDLK_ESCAPE)             \
	XX(SDLK_BACKSPACE)          \
	XX(SDLK_TAB)                \
	XX(SDLK_SPACE)              \
	XX(SDLK_EXCLAIM)            \
	XX(SDLK_QUOTEDBL)           \
	XX(SDLK_HASH)               \
	XX(SDLK_PERCENT)            \
	XX(SDLK_DOLLAR)             \
	XX(SDLK_AMPERSAND)          \
	XX(SDLK_QUOTE)              \
	XX(SDLK_LEFTPAREN)          \
	XX(SDLK_RIGHTPAREN)         \
	XX(SDLK_ASTERISK)           \
	XX(SDLK_PLUS)               \
	XX(SDLK_COMMA)              \
	XX(SDLK_MINUS)              \
	XX(SDLK_PERIOD)             \
	XX(SDLK_SLASH)              \
	XX(SDLK_0)                  \
	XX(SDLK_1)                  \
	XX(SDLK_2)                  \
	XX(SDLK_3)                  \
	XX(SDLK_4)                  \
	XX(SDLK_5)                  \
	XX(SDLK_6)                  \
	XX(SDLK_7)                  \
	XX(SDLK_8)                  \
	XX(SDLK_9)                  \
	XX(SDLK_COLON)              \
	XX(SDLK_SEMICOLON)          \
	XX(SDLK_LESS)               \
	XX(SDLK_EQUALS)             \
	XX(SDLK_GREATER)            \
	XX(SDLK_QUESTION)           \
	XX(SDLK_AT)                 \
	XX(SDLK_LEFTBRACKET)        \
	XX(SDLK_BACKSLASH)          \
	XX(SDLK_RIGHTBRACKET)       \
	XX(SDLK_CARET)              \
	XX(SDLK_UNDERSCORE)         \
	XX(SDLK_BACKQUOTE)          \
	XX(SDLK_a)                  \
	XX(SDLK_b)                  \
	XX(SDLK_c)                  \
	XX(SDLK_d)                  \
	XX(SDLK_e)                  \
	XX(SDLK_f)                  \
	XX(SDLK_g)                  \
	XX(SDLK_h)                  \
	XX(SDLK_i)                  \
	XX(SDLK_j)                  \
	XX(SDLK_k)                  \
	XX(SDLK_l)                  \
	XX(SDLK_m)                  \
	XX(SDLK_n)                  \
	XX(SDLK_o)                  \
	XX(SDLK_p)                  \
	XX(SDLK_q)                  \
	XX(SDLK_r)                  \
	XX(SDLK_s)                  \
	XX(SDLK_t)                  \
	XX(SDLK_u)                  \
	XX(SDLK_v)                  \
	XX(SDLK_w)                  \
	XX(SDLK_x)                  \
	XX(SDLK_y)                  \
	XX(SDLK_z)                  \
	XX(SDLK_CAPSLOCK)           \
	XX(SDLK_F1)                 \
	XX(SDLK_F2)                 \
	XX(SDLK_F3)                 \
	XX(SDLK_F4)                 \
	XX(SDLK_F5)                 \
	XX(SDLK_F6)                 \
	XX(SDLK_F7)                 \
	XX(SDLK_F8)                 \
	XX(SDLK_F9)                 \
	XX(SDLK_F10)                \
	XX(SDLK_F11)                \
	XX(SDLK_F12)                \
	XX(SDLK_PRINTSCREEN)        \
	XX(SDLK_SCROLLLOCK)         \
	XX(SDLK_PAUSE)              \
	XX(SDLK_INSERT)             \
	XX(SDLK_HOME)               \
	XX(SDLK_PAGEUP)             \
	XX(SDLK_DELETE)             \
	XX(SDLK_END)                \
	XX(SDLK_PAGEDOWN)           \
	XX(SDLK_RIGHT)              \
	XX(SDLK_LEFT)               \
	XX(SDLK_DOWN)               \
	XX(SDLK_UP)                 \
	XX(SDLK_NUMLOCKCLEAR)       \
	XX(SDLK_KP_DIVIDE)          \
	XX(SDLK_KP_MULTIPLY)        \
	XX(SDLK_KP_MINUS)           \
	XX(SDLK_KP_PLUS)            \
	XX(SDLK_KP_ENTER)           \
	XX(SDLK_KP_1)               \
	XX(SDLK_KP_2)               \
	XX(SDLK_KP_3)               \
	XX(SDLK_KP_4)               \
	XX(SDLK_KP_5)               \
	XX(SDLK_KP_6)               \
	XX(SDLK_KP_7)               \
	XX(SDLK_KP_8)               \
	XX(SDLK_KP_9)               \
	XX(SDLK_KP_0)               \
	XX(SDLK_KP_PERIOD)          \
	XX(SDLK_APPLICATION)        \
	XX(SDLK_POWER)              \
	XX(SDLK_KP_EQUALS)          \
	XX(SDLK_F13)                \
	XX(SDLK_F14)                \
	XX(SDLK_F15)                \
	XX(SDLK_F16)                \
	XX(SDLK_F17)                \
	XX(SDLK_F18)                \
	XX(SDLK_F19)                \
	XX(SDLK_F20)                \
	XX(SDLK_F21)                \
	XX(SDLK_F22)                \
	XX(SDLK_F23)                \
	XX(SDLK_F24)                \
	XX(SDLK_EXECUTE)            \
	XX(SDLK_HELP)               \
	XX(SDLK_MENU)               \
	XX(SDLK_SELECT)             \
	XX(SDLK_STOP)               \
	XX(SDLK_AGAIN)              \
	XX(SDLK_UNDO)               \
	XX(SDLK_CUT)                \
	XX(SDLK_COPY)               \
	XX(SDLK_PASTE)              \
	XX(SDLK_FIND)               \
	XX(SDLK_MUTE)               \
	XX(SDLK_VOLUMEUP)           \
	XX(SDLK_VOLUMEDOWN)         \
	XX(SDLK_KP_COMMA)           \
	XX(SDLK_KP_EQUALSAS400)     \
	XX(SDLK_ALTERASE)           \
	XX(SDLK_SYSREQ)             \
	XX(SDLK_CANCEL)             \
	XX(SDLK_CLEAR)              \
	XX(SDLK_PRIOR)              \
	XX(SDLK_RETURN2)            \
	XX(SDLK_SEPARATOR)          \
	XX(SDLK_OUT)                \
	XX(SDLK_OPER)               \
	XX(SDLK_CLEARAGAIN)         \
	XX(SDLK_CRSEL)              \
	XX(SDLK_EXSEL)              \
	XX(SDLK_KP_00)              \
	XX(SDLK_KP_000)             \
	XX(SDLK_THOUSANDSSEPARATOR) \
	XX(SDLK_DECIMALSEPARATOR)   \
	XX(SDLK_CURRENCYUNIT)       \
	XX(SDLK_CURRENCYSUBUNIT)    \
	XX(SDLK_KP_LEFTPAREN)       \
	XX(SDLK_KP_RIGHTPAREN)      \
	XX(SDLK_KP_LEFTBRACE)       \
	XX(SDLK_KP_RIGHTBRACE)      \
	XX(SDLK_KP_TAB)             \
	XX(SDLK_KP_BACKSPACE)       \
	XX(SDLK_KP_A)               \
	XX(SDLK_KP_B)               \
	XX(SDLK_KP_C)               \
	XX(SDLK_KP_D)               \
	XX(SDLK_KP_E)               \
	XX(SDLK_KP_F)               \
	XX(SDLK_KP_XOR)             \
	XX(SDLK_KP_POWER)           \
	XX(SDLK_KP_PERCENT)         \
	XX(SDLK_KP_LESS)            \
	XX(SDLK_KP_GREATER)         \
	XX(SDLK_KP_AMPERSAND)       \
	XX(SDLK_KP_DBLAMPERSAND)    \
	XX(SDLK_KP_VERTICALBAR)     \
	XX(SDLK_KP_DBLVERTICALBAR)  \
	XX(SDLK_KP_COLON)           \
	XX(SDLK_KP_HASH)            \
	XX(SDLK_KP_SPACE)           \
	XX(SDLK_KP_AT)              \
	XX(SDLK_KP_EXCLAM)          \
	XX(SDLK_KP_MEMSTORE)        \
	XX(SDLK_KP_MEMRECALL)       \
	XX(SDLK_KP_MEMCLEAR)        \
	XX(SDLK_KP_MEMADD)          \
	XX(SDLK_KP_MEMSUBTRACT)     \
	XX(SDLK_KP_MEMMULTIPLY)     \
	XX(SDLK_KP_MEMDIVIDE)       \
	XX(SDLK_KP_PLUSMINUS)       \
	XX(SDLK_KP_CLEAR)           \
	XX(SDLK_KP_CLEARENTRY)      \
	XX(SDLK_KP_BINARY)          \
	XX(SDLK_KP_OCTAL)           \
	XX(SDLK_KP_DECIMAL)         \
	XX(SDLK_KP_HEXADECIMAL)     \
	XX(SDLK_LCTRL)              \
	XX(SDLK_LSHIFT)             \
	XX(SDLK_LALT)               \
	XX(SDLK_LGUI)               \
	XX(SDLK_RCTRL)              \
	XX(SDLK_RSHIFT)             \
	XX(SDLK_RALT)               \
	XX(SDLK_RGUI)               \
	XX(SDLK_MODE)               \
	XX(SDLK_AUDIONEXT)          \
	XX(SDLK_AUDIOPREV)          \
	XX(SDLK_AUDIOSTOP)          \
	XX(SDLK_AUDIOPLAY)          \
	XX(SDLK_AUDIOMUTE)          \
	XX(SDLK_MEDIASELECT)        \
	XX(SDLK_WWW)                \
	XX(SDLK_MAIL)               \
	XX(SDLK_CALCULATOR)         \
	XX(SDLK_COMPUTER)           \
	XX(SDLK_AC_SEARCH)          \
	XX(SDLK_AC_HOME)            \
	XX(SDLK_AC_BACK)            \
	XX(SDLK_AC_FORWARD)         \
	XX(SDLK_AC_STOP)            \
	XX(SDLK_AC_REFRESH)         \
	XX(SDLK_AC_BOOKMARKS)       \
	XX(SDLK_BRIGHTNESSDOWN)     \
	XX(SDLK_BRIGHTNESSUP)       \
	XX(SDLK_DISPLAYSWITCH)      \
	XX(SDLK_KBDILLUMTOGGLE)     \
	XX(SDLK_KBDILLUMDOWN)       \
	XX(SDLK_KBDILLUMUP)         \
	XX(SDLK_EJECT)              \
	XX(SDLK_SLEEP)              \
	XX(SDLK_APP1)               \
	XX(SDLK_APP2)               \
	XX(SDLK_AUDIOREWIND)        \
	XX(SDLK_AUDIOFASTFORWARD)

// this is horrible, but because I need SDL_GetKeyName and I haven't initialized SDL
// also technically I might stop using SDL but I would still use SDL's names.

// NOLINTNEXTLINE
SDL_Keycode find_sdl_keycode(const char* string, size_t size)
{
#define XX(code)                                                   \
	if(strlen(#code) == size && strncmp(string, #code, size) == 0) \
	{                                                              \
		return code;                                               \
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
	case(code): return #code;
	switch(key)
	{
		MY_KEYCODE_MAP(XX)
	}
#undef XX
	return "UNKNOWN_KEY_CODE????";
}