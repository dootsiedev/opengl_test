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
// Maybe what I need is a way to make cvars dependant on other cvars, so if this cvar is ==1, disable this cvar.
// but this is tricky to handle because of static initialization order, I think.

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

// returns = 0 for no modifier
Uint16 find_sdl_mod(const char* string, size_t size)
{
#define XSTRNCMP(code)                                                 \
	do                                                                 \
	{                                                                  \
		if(strlen(#code) == size && strncmp(string, #code, size) == 0) \
		{                                                              \
			return code;                                               \
		}                                                              \
	} while(0)
	XSTRNCMP(KMOD_LSHIFT);
	XSTRNCMP(KMOD_RSHIFT);
	XSTRNCMP(KMOD_LCTRL);
	XSTRNCMP(KMOD_RCTRL);
	XSTRNCMP(KMOD_LALT);
	XSTRNCMP(KMOD_RALT);
	XSTRNCMP(KMOD_CTRL);
	XSTRNCMP(KMOD_SHIFT);
	XSTRNCMP(KMOD_ALT);
#undef XSTRNCMP
	return 0;
}

const char* get_sdl_mod_name(Uint16 mod)
{
#define XSTRNCMP(code)    \
	do                    \
	{                     \
		if((code) == mod) \
		{                 \
			return #code; \
		}                 \
	} while(0)
	XSTRNCMP(KMOD_LSHIFT);
	XSTRNCMP(KMOD_RSHIFT);
	XSTRNCMP(KMOD_LCTRL);
	XSTRNCMP(KMOD_RCTRL);
	XSTRNCMP(KMOD_LALT);
	XSTRNCMP(KMOD_RALT);
	XSTRNCMP(KMOD_CTRL);
	XSTRNCMP(KMOD_SHIFT);
	XSTRNCMP(KMOD_ALT);
#undef XSTRNCMP
	return NULL;
}

// this is horrible, but because I need SDL_GetKeyName and I haven't initialized SDL
// also technically I might stop using SDL but I would still use SDL's names.

// NOLINTNEXTLINE
SDL_Keycode find_sdl_keycode(const char* string, size_t size)
{
#define XSTRNCMP(code)                                                 \
	do                                                                 \
	{                                                                  \
		if(strlen(#code) == size && strncmp(string, #code, size) == 0) \
		{                                                              \
			return code;                                               \
		}                                                              \
	} while(0)
	XSTRNCMP(SDLK_RETURN);
	XSTRNCMP(SDLK_ESCAPE);
	XSTRNCMP(SDLK_BACKSPACE);
	XSTRNCMP(SDLK_TAB);
	XSTRNCMP(SDLK_SPACE);
	XSTRNCMP(SDLK_EXCLAIM);
	XSTRNCMP(SDLK_QUOTEDBL);
	XSTRNCMP(SDLK_HASH);
	XSTRNCMP(SDLK_PERCENT);
	XSTRNCMP(SDLK_DOLLAR);
	XSTRNCMP(SDLK_AMPERSAND);
	XSTRNCMP(SDLK_QUOTE);
	XSTRNCMP(SDLK_LEFTPAREN);
	XSTRNCMP(SDLK_RIGHTPAREN);
	XSTRNCMP(SDLK_ASTERISK);
	XSTRNCMP(SDLK_PLUS);
	XSTRNCMP(SDLK_COMMA);
	XSTRNCMP(SDLK_MINUS);
	XSTRNCMP(SDLK_PERIOD);
	XSTRNCMP(SDLK_SLASH);
	XSTRNCMP(SDLK_0);
	XSTRNCMP(SDLK_1);
	XSTRNCMP(SDLK_2);
	XSTRNCMP(SDLK_3);
	XSTRNCMP(SDLK_4);
	XSTRNCMP(SDLK_5);
	XSTRNCMP(SDLK_6);
	XSTRNCMP(SDLK_7);
	XSTRNCMP(SDLK_8);
	XSTRNCMP(SDLK_9);
	XSTRNCMP(SDLK_COLON);
	XSTRNCMP(SDLK_SEMICOLON);
	XSTRNCMP(SDLK_LESS);
	XSTRNCMP(SDLK_EQUALS);
	XSTRNCMP(SDLK_GREATER);
	XSTRNCMP(SDLK_QUESTION);
	XSTRNCMP(SDLK_AT);
	XSTRNCMP(SDLK_LEFTBRACKET);
	XSTRNCMP(SDLK_BACKSLASH);
	XSTRNCMP(SDLK_RIGHTBRACKET);
	XSTRNCMP(SDLK_CARET);
	XSTRNCMP(SDLK_UNDERSCORE);
	XSTRNCMP(SDLK_BACKQUOTE);
	XSTRNCMP(SDLK_a);
	XSTRNCMP(SDLK_b);
	XSTRNCMP(SDLK_c);
	XSTRNCMP(SDLK_d);
	XSTRNCMP(SDLK_e);
	XSTRNCMP(SDLK_f);
	XSTRNCMP(SDLK_g);
	XSTRNCMP(SDLK_h);
	XSTRNCMP(SDLK_i);
	XSTRNCMP(SDLK_j);
	XSTRNCMP(SDLK_k);
	XSTRNCMP(SDLK_l);
	XSTRNCMP(SDLK_m);
	XSTRNCMP(SDLK_n);
	XSTRNCMP(SDLK_o);
	XSTRNCMP(SDLK_p);
	XSTRNCMP(SDLK_q);
	XSTRNCMP(SDLK_r);
	XSTRNCMP(SDLK_s);
	XSTRNCMP(SDLK_t);
	XSTRNCMP(SDLK_u);
	XSTRNCMP(SDLK_v);
	XSTRNCMP(SDLK_w);
	XSTRNCMP(SDLK_x);
	XSTRNCMP(SDLK_y);
	XSTRNCMP(SDLK_z);
	XSTRNCMP(SDLK_CAPSLOCK);
	XSTRNCMP(SDLK_F1);
	XSTRNCMP(SDLK_F2);
	XSTRNCMP(SDLK_F3);
	XSTRNCMP(SDLK_F4);
	XSTRNCMP(SDLK_F5);
	XSTRNCMP(SDLK_F6);
	XSTRNCMP(SDLK_F7);
	XSTRNCMP(SDLK_F8);
	XSTRNCMP(SDLK_F9);
	XSTRNCMP(SDLK_F10);
	XSTRNCMP(SDLK_F11);
	XSTRNCMP(SDLK_F12);
	XSTRNCMP(SDLK_PRINTSCREEN);
	XSTRNCMP(SDLK_SCROLLLOCK);
	XSTRNCMP(SDLK_PAUSE);
	XSTRNCMP(SDLK_INSERT);
	XSTRNCMP(SDLK_HOME);
	XSTRNCMP(SDLK_PAGEUP);
	XSTRNCMP(SDLK_DELETE);
	XSTRNCMP(SDLK_END);
	XSTRNCMP(SDLK_PAGEDOWN);
	XSTRNCMP(SDLK_RIGHT);
	XSTRNCMP(SDLK_LEFT);
	XSTRNCMP(SDLK_DOWN);
	XSTRNCMP(SDLK_UP);
	XSTRNCMP(SDLK_NUMLOCKCLEAR);
	XSTRNCMP(SDLK_KP_DIVIDE);
	XSTRNCMP(SDLK_KP_MULTIPLY);
	XSTRNCMP(SDLK_KP_MINUS);
	XSTRNCMP(SDLK_KP_PLUS);
	XSTRNCMP(SDLK_KP_ENTER);
	XSTRNCMP(SDLK_KP_1);
	XSTRNCMP(SDLK_KP_2);
	XSTRNCMP(SDLK_KP_3);
	XSTRNCMP(SDLK_KP_4);
	XSTRNCMP(SDLK_KP_5);
	XSTRNCMP(SDLK_KP_6);
	XSTRNCMP(SDLK_KP_7);
	XSTRNCMP(SDLK_KP_8);
	XSTRNCMP(SDLK_KP_9);
	XSTRNCMP(SDLK_KP_0);
	XSTRNCMP(SDLK_KP_PERIOD);
	XSTRNCMP(SDLK_APPLICATION);
	XSTRNCMP(SDLK_POWER);
	XSTRNCMP(SDLK_KP_EQUALS);
	XSTRNCMP(SDLK_F13);
	XSTRNCMP(SDLK_F14);
	XSTRNCMP(SDLK_F15);
	XSTRNCMP(SDLK_F16);
	XSTRNCMP(SDLK_F17);
	XSTRNCMP(SDLK_F18);
	XSTRNCMP(SDLK_F19);
	XSTRNCMP(SDLK_F20);
	XSTRNCMP(SDLK_F21);
	XSTRNCMP(SDLK_F22);
	XSTRNCMP(SDLK_F23);
	XSTRNCMP(SDLK_F24);
	XSTRNCMP(SDLK_EXECUTE);
	XSTRNCMP(SDLK_HELP);
	XSTRNCMP(SDLK_MENU);
	XSTRNCMP(SDLK_SELECT);
	XSTRNCMP(SDLK_STOP);
	XSTRNCMP(SDLK_AGAIN);
	XSTRNCMP(SDLK_UNDO);
	XSTRNCMP(SDLK_CUT);
	XSTRNCMP(SDLK_COPY);
	XSTRNCMP(SDLK_PASTE);
	XSTRNCMP(SDLK_FIND);
	XSTRNCMP(SDLK_MUTE);
	XSTRNCMP(SDLK_VOLUMEUP);
	XSTRNCMP(SDLK_VOLUMEDOWN);
	XSTRNCMP(SDLK_KP_COMMA);
	XSTRNCMP(SDLK_KP_EQUALSAS400);
	XSTRNCMP(SDLK_ALTERASE);
	XSTRNCMP(SDLK_SYSREQ);
	XSTRNCMP(SDLK_CANCEL);
	XSTRNCMP(SDLK_CLEAR);
	XSTRNCMP(SDLK_PRIOR);
	XSTRNCMP(SDLK_RETURN2);
	XSTRNCMP(SDLK_SEPARATOR);
	XSTRNCMP(SDLK_OUT);
	XSTRNCMP(SDLK_OPER);
	XSTRNCMP(SDLK_CLEARAGAIN);
	XSTRNCMP(SDLK_CRSEL);
	XSTRNCMP(SDLK_EXSEL);
	XSTRNCMP(SDLK_KP_00);
	XSTRNCMP(SDLK_KP_000);
	XSTRNCMP(SDLK_THOUSANDSSEPARATOR);
	XSTRNCMP(SDLK_DECIMALSEPARATOR);
	XSTRNCMP(SDLK_CURRENCYUNIT);
	XSTRNCMP(SDLK_CURRENCYSUBUNIT);
	XSTRNCMP(SDLK_KP_LEFTPAREN);
	XSTRNCMP(SDLK_KP_RIGHTPAREN);
	XSTRNCMP(SDLK_KP_LEFTBRACE);
	XSTRNCMP(SDLK_KP_RIGHTBRACE);
	XSTRNCMP(SDLK_KP_TAB);
	XSTRNCMP(SDLK_KP_BACKSPACE);
	XSTRNCMP(SDLK_KP_A);
	XSTRNCMP(SDLK_KP_B);
	XSTRNCMP(SDLK_KP_C);
	XSTRNCMP(SDLK_KP_D);
	XSTRNCMP(SDLK_KP_E);
	XSTRNCMP(SDLK_KP_F);
	XSTRNCMP(SDLK_KP_XOR);
	XSTRNCMP(SDLK_KP_POWER);
	XSTRNCMP(SDLK_KP_PERCENT);
	XSTRNCMP(SDLK_KP_LESS);
	XSTRNCMP(SDLK_KP_GREATER);
	XSTRNCMP(SDLK_KP_AMPERSAND);
	XSTRNCMP(SDLK_KP_DBLAMPERSAND);
	XSTRNCMP(SDLK_KP_VERTICALBAR);
	XSTRNCMP(SDLK_KP_DBLVERTICALBAR);
	XSTRNCMP(SDLK_KP_COLON);
	XSTRNCMP(SDLK_KP_HASH);
	XSTRNCMP(SDLK_KP_SPACE);
	XSTRNCMP(SDLK_KP_AT);
	XSTRNCMP(SDLK_KP_EXCLAM);
	XSTRNCMP(SDLK_KP_MEMSTORE);
	XSTRNCMP(SDLK_KP_MEMRECALL);
	XSTRNCMP(SDLK_KP_MEMCLEAR);
	XSTRNCMP(SDLK_KP_MEMADD);
	XSTRNCMP(SDLK_KP_MEMSUBTRACT);
	XSTRNCMP(SDLK_KP_MEMMULTIPLY);
	XSTRNCMP(SDLK_KP_MEMDIVIDE);
	XSTRNCMP(SDLK_KP_PLUSMINUS);
	XSTRNCMP(SDLK_KP_CLEAR);
	XSTRNCMP(SDLK_KP_CLEARENTRY);
	XSTRNCMP(SDLK_KP_BINARY);
	XSTRNCMP(SDLK_KP_OCTAL);
	XSTRNCMP(SDLK_KP_DECIMAL);
	XSTRNCMP(SDLK_KP_HEXADECIMAL);
	XSTRNCMP(SDLK_LCTRL);
	XSTRNCMP(SDLK_LSHIFT);
	XSTRNCMP(SDLK_LALT);
	XSTRNCMP(SDLK_LGUI);
	XSTRNCMP(SDLK_RCTRL);
	XSTRNCMP(SDLK_RSHIFT);
	XSTRNCMP(SDLK_RALT);
	XSTRNCMP(SDLK_RGUI);
	XSTRNCMP(SDLK_MODE);
	XSTRNCMP(SDLK_AUDIONEXT);
	XSTRNCMP(SDLK_AUDIOPREV);
	XSTRNCMP(SDLK_AUDIOSTOP);
	XSTRNCMP(SDLK_AUDIOPLAY);
	XSTRNCMP(SDLK_AUDIOMUTE);
	XSTRNCMP(SDLK_MEDIASELECT);
	XSTRNCMP(SDLK_WWW);
	XSTRNCMP(SDLK_MAIL);
	XSTRNCMP(SDLK_CALCULATOR);
	XSTRNCMP(SDLK_COMPUTER);
	XSTRNCMP(SDLK_AC_SEARCH);
	XSTRNCMP(SDLK_AC_HOME);
	XSTRNCMP(SDLK_AC_BACK);
	XSTRNCMP(SDLK_AC_FORWARD);
	XSTRNCMP(SDLK_AC_STOP);
	XSTRNCMP(SDLK_AC_REFRESH);
	XSTRNCMP(SDLK_AC_BOOKMARKS);
	XSTRNCMP(SDLK_BRIGHTNESSDOWN);
	XSTRNCMP(SDLK_BRIGHTNESSUP);
	XSTRNCMP(SDLK_DISPLAYSWITCH);
	XSTRNCMP(SDLK_KBDILLUMTOGGLE);
	XSTRNCMP(SDLK_KBDILLUMDOWN);
	XSTRNCMP(SDLK_KBDILLUMUP);
	XSTRNCMP(SDLK_EJECT);
	XSTRNCMP(SDLK_SLEEP);
	XSTRNCMP(SDLK_APP1);
	XSTRNCMP(SDLK_APP2);
	XSTRNCMP(SDLK_AUDIOREWIND);
	XSTRNCMP(SDLK_AUDIOFASTFORWARD);
	return -1;
#undef XSTRNCMP
}

// I know I don't need to copy-paste this if I used the X macro
// but I didn't expect SDL_GetKeyName to not return SDLK_ formatted strings!!!!
// NOLINTNEXTLINE
const char* get_sdl_key_name(SDL_Keycode key)
{
#define XSTRNCMP(code)    \
	do                    \
	{                     \
		if((code) == key) \
		{                 \
			return #code; \
		}                 \
	} while(0)
	XSTRNCMP(SDLK_RETURN);
	XSTRNCMP(SDLK_ESCAPE);
	XSTRNCMP(SDLK_BACKSPACE);
	XSTRNCMP(SDLK_TAB);
	XSTRNCMP(SDLK_SPACE);
	XSTRNCMP(SDLK_EXCLAIM);
	XSTRNCMP(SDLK_QUOTEDBL);
	XSTRNCMP(SDLK_HASH);
	XSTRNCMP(SDLK_PERCENT);
	XSTRNCMP(SDLK_DOLLAR);
	XSTRNCMP(SDLK_AMPERSAND);
	XSTRNCMP(SDLK_QUOTE);
	XSTRNCMP(SDLK_LEFTPAREN);
	XSTRNCMP(SDLK_RIGHTPAREN);
	XSTRNCMP(SDLK_ASTERISK);
	XSTRNCMP(SDLK_PLUS);
	XSTRNCMP(SDLK_COMMA);
	XSTRNCMP(SDLK_MINUS);
	XSTRNCMP(SDLK_PERIOD);
	XSTRNCMP(SDLK_SLASH);
	XSTRNCMP(SDLK_0);
	XSTRNCMP(SDLK_1);
	XSTRNCMP(SDLK_2);
	XSTRNCMP(SDLK_3);
	XSTRNCMP(SDLK_4);
	XSTRNCMP(SDLK_5);
	XSTRNCMP(SDLK_6);
	XSTRNCMP(SDLK_7);
	XSTRNCMP(SDLK_8);
	XSTRNCMP(SDLK_9);
	XSTRNCMP(SDLK_COLON);
	XSTRNCMP(SDLK_SEMICOLON);
	XSTRNCMP(SDLK_LESS);
	XSTRNCMP(SDLK_EQUALS);
	XSTRNCMP(SDLK_GREATER);
	XSTRNCMP(SDLK_QUESTION);
	XSTRNCMP(SDLK_AT);
	XSTRNCMP(SDLK_LEFTBRACKET);
	XSTRNCMP(SDLK_BACKSLASH);
	XSTRNCMP(SDLK_RIGHTBRACKET);
	XSTRNCMP(SDLK_CARET);
	XSTRNCMP(SDLK_UNDERSCORE);
	XSTRNCMP(SDLK_BACKQUOTE);
	XSTRNCMP(SDLK_a);
	XSTRNCMP(SDLK_b);
	XSTRNCMP(SDLK_c);
	XSTRNCMP(SDLK_d);
	XSTRNCMP(SDLK_e);
	XSTRNCMP(SDLK_f);
	XSTRNCMP(SDLK_g);
	XSTRNCMP(SDLK_h);
	XSTRNCMP(SDLK_i);
	XSTRNCMP(SDLK_j);
	XSTRNCMP(SDLK_k);
	XSTRNCMP(SDLK_l);
	XSTRNCMP(SDLK_m);
	XSTRNCMP(SDLK_n);
	XSTRNCMP(SDLK_o);
	XSTRNCMP(SDLK_p);
	XSTRNCMP(SDLK_q);
	XSTRNCMP(SDLK_r);
	XSTRNCMP(SDLK_s);
	XSTRNCMP(SDLK_t);
	XSTRNCMP(SDLK_u);
	XSTRNCMP(SDLK_v);
	XSTRNCMP(SDLK_w);
	XSTRNCMP(SDLK_x);
	XSTRNCMP(SDLK_y);
	XSTRNCMP(SDLK_z);
	XSTRNCMP(SDLK_CAPSLOCK);
	XSTRNCMP(SDLK_F1);
	XSTRNCMP(SDLK_F2);
	XSTRNCMP(SDLK_F3);
	XSTRNCMP(SDLK_F4);
	XSTRNCMP(SDLK_F5);
	XSTRNCMP(SDLK_F6);
	XSTRNCMP(SDLK_F7);
	XSTRNCMP(SDLK_F8);
	XSTRNCMP(SDLK_F9);
	XSTRNCMP(SDLK_F10);
	XSTRNCMP(SDLK_F11);
	XSTRNCMP(SDLK_F12);
	XSTRNCMP(SDLK_PRINTSCREEN);
	XSTRNCMP(SDLK_SCROLLLOCK);
	XSTRNCMP(SDLK_PAUSE);
	XSTRNCMP(SDLK_INSERT);
	XSTRNCMP(SDLK_HOME);
	XSTRNCMP(SDLK_PAGEUP);
	XSTRNCMP(SDLK_DELETE);
	XSTRNCMP(SDLK_END);
	XSTRNCMP(SDLK_PAGEDOWN);
	XSTRNCMP(SDLK_RIGHT);
	XSTRNCMP(SDLK_LEFT);
	XSTRNCMP(SDLK_DOWN);
	XSTRNCMP(SDLK_UP);
	XSTRNCMP(SDLK_NUMLOCKCLEAR);
	XSTRNCMP(SDLK_KP_DIVIDE);
	XSTRNCMP(SDLK_KP_MULTIPLY);
	XSTRNCMP(SDLK_KP_MINUS);
	XSTRNCMP(SDLK_KP_PLUS);
	XSTRNCMP(SDLK_KP_ENTER);
	XSTRNCMP(SDLK_KP_1);
	XSTRNCMP(SDLK_KP_2);
	XSTRNCMP(SDLK_KP_3);
	XSTRNCMP(SDLK_KP_4);
	XSTRNCMP(SDLK_KP_5);
	XSTRNCMP(SDLK_KP_6);
	XSTRNCMP(SDLK_KP_7);
	XSTRNCMP(SDLK_KP_8);
	XSTRNCMP(SDLK_KP_9);
	XSTRNCMP(SDLK_KP_0);
	XSTRNCMP(SDLK_KP_PERIOD);
	XSTRNCMP(SDLK_APPLICATION);
	XSTRNCMP(SDLK_POWER);
	XSTRNCMP(SDLK_KP_EQUALS);
	XSTRNCMP(SDLK_F13);
	XSTRNCMP(SDLK_F14);
	XSTRNCMP(SDLK_F15);
	XSTRNCMP(SDLK_F16);
	XSTRNCMP(SDLK_F17);
	XSTRNCMP(SDLK_F18);
	XSTRNCMP(SDLK_F19);
	XSTRNCMP(SDLK_F20);
	XSTRNCMP(SDLK_F21);
	XSTRNCMP(SDLK_F22);
	XSTRNCMP(SDLK_F23);
	XSTRNCMP(SDLK_F24);
	XSTRNCMP(SDLK_EXECUTE);
	XSTRNCMP(SDLK_HELP);
	XSTRNCMP(SDLK_MENU);
	XSTRNCMP(SDLK_SELECT);
	XSTRNCMP(SDLK_STOP);
	XSTRNCMP(SDLK_AGAIN);
	XSTRNCMP(SDLK_UNDO);
	XSTRNCMP(SDLK_CUT);
	XSTRNCMP(SDLK_COPY);
	XSTRNCMP(SDLK_PASTE);
	XSTRNCMP(SDLK_FIND);
	XSTRNCMP(SDLK_MUTE);
	XSTRNCMP(SDLK_VOLUMEUP);
	XSTRNCMP(SDLK_VOLUMEDOWN);
	XSTRNCMP(SDLK_KP_COMMA);
	XSTRNCMP(SDLK_KP_EQUALSAS400);
	XSTRNCMP(SDLK_ALTERASE);
	XSTRNCMP(SDLK_SYSREQ);
	XSTRNCMP(SDLK_CANCEL);
	XSTRNCMP(SDLK_CLEAR);
	XSTRNCMP(SDLK_PRIOR);
	XSTRNCMP(SDLK_RETURN2);
	XSTRNCMP(SDLK_SEPARATOR);
	XSTRNCMP(SDLK_OUT);
	XSTRNCMP(SDLK_OPER);
	XSTRNCMP(SDLK_CLEARAGAIN);
	XSTRNCMP(SDLK_CRSEL);
	XSTRNCMP(SDLK_EXSEL);
	XSTRNCMP(SDLK_KP_00);
	XSTRNCMP(SDLK_KP_000);
	XSTRNCMP(SDLK_THOUSANDSSEPARATOR);
	XSTRNCMP(SDLK_DECIMALSEPARATOR);
	XSTRNCMP(SDLK_CURRENCYUNIT);
	XSTRNCMP(SDLK_CURRENCYSUBUNIT);
	XSTRNCMP(SDLK_KP_LEFTPAREN);
	XSTRNCMP(SDLK_KP_RIGHTPAREN);
	XSTRNCMP(SDLK_KP_LEFTBRACE);
	XSTRNCMP(SDLK_KP_RIGHTBRACE);
	XSTRNCMP(SDLK_KP_TAB);
	XSTRNCMP(SDLK_KP_BACKSPACE);
	XSTRNCMP(SDLK_KP_A);
	XSTRNCMP(SDLK_KP_B);
	XSTRNCMP(SDLK_KP_C);
	XSTRNCMP(SDLK_KP_D);
	XSTRNCMP(SDLK_KP_E);
	XSTRNCMP(SDLK_KP_F);
	XSTRNCMP(SDLK_KP_XOR);
	XSTRNCMP(SDLK_KP_POWER);
	XSTRNCMP(SDLK_KP_PERCENT);
	XSTRNCMP(SDLK_KP_LESS);
	XSTRNCMP(SDLK_KP_GREATER);
	XSTRNCMP(SDLK_KP_AMPERSAND);
	XSTRNCMP(SDLK_KP_DBLAMPERSAND);
	XSTRNCMP(SDLK_KP_VERTICALBAR);
	XSTRNCMP(SDLK_KP_DBLVERTICALBAR);
	XSTRNCMP(SDLK_KP_COLON);
	XSTRNCMP(SDLK_KP_HASH);
	XSTRNCMP(SDLK_KP_SPACE);
	XSTRNCMP(SDLK_KP_AT);
	XSTRNCMP(SDLK_KP_EXCLAM);
	XSTRNCMP(SDLK_KP_MEMSTORE);
	XSTRNCMP(SDLK_KP_MEMRECALL);
	XSTRNCMP(SDLK_KP_MEMCLEAR);
	XSTRNCMP(SDLK_KP_MEMADD);
	XSTRNCMP(SDLK_KP_MEMSUBTRACT);
	XSTRNCMP(SDLK_KP_MEMMULTIPLY);
	XSTRNCMP(SDLK_KP_MEMDIVIDE);
	XSTRNCMP(SDLK_KP_PLUSMINUS);
	XSTRNCMP(SDLK_KP_CLEAR);
	XSTRNCMP(SDLK_KP_CLEARENTRY);
	XSTRNCMP(SDLK_KP_BINARY);
	XSTRNCMP(SDLK_KP_OCTAL);
	XSTRNCMP(SDLK_KP_DECIMAL);
	XSTRNCMP(SDLK_KP_HEXADECIMAL);
	XSTRNCMP(SDLK_LCTRL);
	XSTRNCMP(SDLK_LSHIFT);
	XSTRNCMP(SDLK_LALT);
	XSTRNCMP(SDLK_LGUI);
	XSTRNCMP(SDLK_RCTRL);
	XSTRNCMP(SDLK_RSHIFT);
	XSTRNCMP(SDLK_RALT);
	XSTRNCMP(SDLK_RGUI);
	XSTRNCMP(SDLK_MODE);
	XSTRNCMP(SDLK_AUDIONEXT);
	XSTRNCMP(SDLK_AUDIOPREV);
	XSTRNCMP(SDLK_AUDIOSTOP);
	XSTRNCMP(SDLK_AUDIOPLAY);
	XSTRNCMP(SDLK_AUDIOMUTE);
	XSTRNCMP(SDLK_MEDIASELECT);
	XSTRNCMP(SDLK_WWW);
	XSTRNCMP(SDLK_MAIL);
	XSTRNCMP(SDLK_CALCULATOR);
	XSTRNCMP(SDLK_COMPUTER);
	XSTRNCMP(SDLK_AC_SEARCH);
	XSTRNCMP(SDLK_AC_HOME);
	XSTRNCMP(SDLK_AC_BACK);
	XSTRNCMP(SDLK_AC_FORWARD);
	XSTRNCMP(SDLK_AC_STOP);
	XSTRNCMP(SDLK_AC_REFRESH);
	XSTRNCMP(SDLK_AC_BOOKMARKS);
	XSTRNCMP(SDLK_BRIGHTNESSDOWN);
	XSTRNCMP(SDLK_BRIGHTNESSUP);
	XSTRNCMP(SDLK_DISPLAYSWITCH);
	XSTRNCMP(SDLK_KBDILLUMTOGGLE);
	XSTRNCMP(SDLK_KBDILLUMDOWN);
	XSTRNCMP(SDLK_KBDILLUMUP);
	XSTRNCMP(SDLK_EJECT);
	XSTRNCMP(SDLK_SLEEP);
	XSTRNCMP(SDLK_APP1);
	XSTRNCMP(SDLK_APP2);
	XSTRNCMP(SDLK_AUDIOREWIND);
	XSTRNCMP(SDLK_AUDIOFASTFORWARD);
#undef XSTRNCMP
	return "UNKNOWN_KEY_CODE????";
}