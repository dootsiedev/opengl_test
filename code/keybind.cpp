#include "global.h"
#include "keybind.h"

#define MOUSE_LMB_STRING_NAME "LMB"
#define MOUSE_RMB_STRING_NAME "RMB"
#define MOUSE_MMB_STRING_NAME "MMB"

// forward declaration
Uint16 find_sdl_mod(const char* string, size_t size);
SDL_Keycode find_sdl_keycode(const char* string, size_t size);
const char* get_sdl_key_name(SDL_Keycode key);

cvar_key_bind::cvar_key_bind(
	const char* key,
	keybind_entry value,
	const char* comment,
	CVAR_T type,
	const char* file,
	int line)
: V_cvar(key, comment, type, file, line)
{
	auto [it, success] = get_convars().try_emplace(key, *this);
	(void)success;
	// this shouldn't be possible.
	ASSERT(success && "cvar already registered");
	key_bind_count = 1;
	key_binds[0] = value;
}
bool cvar_key_bind::cvar_read(const char* buffer)
{
	ASSERT(buffer);
	// reset values.
	key_bind_count = 0;
	memset(&key_binds, 0, sizeof(key_binds));
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
std::string cvar_key_bind::cvar_write()
{
	std::string out;
	for(size_t i = 0; i < key_bind_count; ++i)
	{
		if(i != 0)
		{
			out += ';';
		}

		if(key_binds[i].mod != 0)
		{
			if((KMOD_CTRL & key_binds[i].mod) != 0)
			{
				out += "KMOD_CTRL;";
			}
			else if((KMOD_SHIFT & key_binds[i].mod) != 0)
			{
				out += "KMOD_SHIFT;";
			}
			else if((KMOD_ALT & key_binds[i].mod) != 0)
			{
				out += "KMOD_ALT;";
			}
			else
			{
				out += "UNKNOWN_MODIFIER????";
			}
		}
		switch(key_binds[i].type)
		{
		case KEYBIND_T::KEY: out += get_sdl_key_name(key_binds[i].key); break;
		case KEYBIND_T::MOUSE:
			switch(key_binds[i].mouse_button)
			{
			case SDL_BUTTON_LEFT: out += MOUSE_LMB_STRING_NAME; break;
			case SDL_BUTTON_RIGHT: out += MOUSE_RMB_STRING_NAME; break;
			case SDL_BUTTON_MIDDLE: out += MOUSE_MMB_STRING_NAME; break;
			default: out += "UNKNOWN_MOUSE_BUTTON????";
			}
			break;
		}
	}
	return out;
}

bool cvar_key_bind::bind_sdl_event(SDL_Event& e, keybind_entry* keybind)
{
	switch(e.type)
	{
        // I use button up because if I try to use the modifier,
        // the modifier would be eaten as the bind before you could press another button.
	case SDL_KEYUP:
		keybind->type = KEYBIND_T::KEY;
		keybind->key = e.key.keysym.sym;
		keybind->mod = e.key.keysym.mod;
		return true;
	case SDL_MOUSEBUTTONUP:
		keybind->type = KEYBIND_T::MOUSE;
		keybind->mouse_button = e.button.button;
		keybind->mod = SDL_GetModState();
		return true;
	}
	return false;
}
keybind_compare_type cvar_key_bind::compare_sdl_event(SDL_Event& e, keybind_compare_type flags)
{
	keybind_compare_type mask = 0;
	for(size_t i = 0; i < key_bind_count; ++i)
	{
		switch(key_binds[i].type)
		{
		case KEYBIND_T::KEY:
			switch(e.type)
			{
			case SDL_KEYDOWN:
                // this doesn't apply to keyup, because then we will never get a keyup!!!
				if(key_binds[i].mod != 0 && (key_binds[i].mod & e.key.keysym.mod) == 0)
				{
					// modifier required.
					break;
				}
				[[fallthrough]];
			case SDL_KEYUP:
				if(e.key.keysym.sym == key_binds[i].key)
				{
					if(e.key.repeat != 0)
					{
						if((flags & KEYBIND_REPEAT) != 0)
						{
							mask |= KEYBIND_REPEAT;
						}
						else
						{
							// no input
							break;
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
				if(key_binds[i].mod != 0 && (key_binds[i].mod & e.key.keysym.mod) == 0)
				{
					// modifier required.
					break;
				}
				[[fallthrough]];
			case SDL_MOUSEBUTTONUP:
				if(e.button.button == key_binds[i].mouse_button)
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
	}
	return KEYBIND_NULL;
}

bool cvar_key_bind::convert_string_to_event(const char* buffer, size_t size)
{
	if(key_bind_count >= MAX_KEY_BINDS)
	{
		serrf("too many key binds, ignoring: %*.s\n", static_cast<int>(size), buffer);
		return false;
	}
	SDL_Keycode key = find_sdl_keycode(buffer, size);
	if(key != -1)
	{
		key_binds[key_bind_count].type = KEYBIND_T::KEY;
		key_binds[key_bind_count].key = key;
		++key_bind_count;
		return true;
	}
	Uint16 mod = find_sdl_mod(buffer, size);
	if(mod != 0)
	{
		// this isn't a key, just modify the "next slot"
		// for the next string that isn't a modifier
		key_binds[key_bind_count].mod |= mod;
		return true;
	}
	if(strncmp(MOUSE_LMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds[key_bind_count].type = KEYBIND_T::MOUSE;
		key_binds[key_bind_count].mouse_button = SDL_BUTTON_LEFT;
		++key_bind_count;
		return true;
	}
	if(strncmp(MOUSE_RMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds[key_bind_count].type = KEYBIND_T::MOUSE;
		key_binds[key_bind_count].mouse_button = SDL_BUTTON_RIGHT;
		++key_bind_count;
		return true;
	}
	if(strncmp(MOUSE_MMB_STRING_NAME, buffer, size) == 0)
	{
		key_binds[key_bind_count].type = KEYBIND_T::MOUSE;
		key_binds[key_bind_count].mouse_button = SDL_BUTTON_MIDDLE;
		++key_bind_count;
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
	XSTRNCMP(KMOD_NONE);
	XSTRNCMP(KMOD_LSHIFT);
	XSTRNCMP(KMOD_RSHIFT);
	XSTRNCMP(KMOD_LCTRL);
	XSTRNCMP(KMOD_RCTRL);
	XSTRNCMP(KMOD_LALT);
	XSTRNCMP(KMOD_RALT);
	XSTRNCMP(KMOD_LGUI);
	XSTRNCMP(KMOD_RGUI);
	XSTRNCMP(KMOD_NUM);
	XSTRNCMP(KMOD_CAPS);
	XSTRNCMP(KMOD_MODE);
	XSTRNCMP(KMOD_SCROLL);
	XSTRNCMP(KMOD_CTRL);
	XSTRNCMP(KMOD_SHIFT);
	XSTRNCMP(KMOD_ALT);
	XSTRNCMP(KMOD_GUI);
#undef XSTRNCMP
	return 0;
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
	XSTRNCMP(SDLK_SOFTLEFT);
	XSTRNCMP(SDLK_SOFTRIGHT);
	XSTRNCMP(SDLK_CALL);
	XSTRNCMP(SDLK_ENDCALL);
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
	XSTRNCMP(SDLK_SOFTLEFT);
	XSTRNCMP(SDLK_SOFTRIGHT);
	XSTRNCMP(SDLK_CALL);
	XSTRNCMP(SDLK_ENDCALL);
	return "UNKNOWN_KEY_CODE????";
#undef XSTRNCMP
}