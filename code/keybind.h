#pragma once

#include "cvar.h"

#include <SDL2/SDL.h>

#define REGISTER_CVAR_KEY_BIND_KEY(key, value, comment, type) \
	cvar_key_bind key(                                        \
		#key, {KEYBIND_T::KEY, 0, value}, comment, type, __FILE__, __LINE__)
#define REGISTER_CVAR_KEY_BIND_MOUSE(key, value, comment, type) \
	cvar_key_bind key(                                          \
		#key, {KEYBIND_T::MOUSE, value, 0}, comment, type, __FILE__, __LINE__)

enum class KEYBIND_T : uint8_t
{
	KEY,
	MOUSE
	// TODO: MMB, scroll wheel, maybe controller?
};
struct keybind_entry
{
    KEYBIND_T type;
    // union-like
    uint8_t mouse_button; // SDL_BUTTON_LEFT or SDL_BUTTON_RIGHT
    SDL_Keycode key;
};

class cvar_key_bind : public V_cvar
{
public:
    enum{
        MAX_KEY_BINDS = 3
        // TODO: more
    };
    
    // SDL_Event is just a very convenient union, 
    // I mainly just use keyboard and mouse
    // but this could support a controller as well.
    keybind_entry key_binds[MAX_KEY_BINDS];
    size_t key_bind_count = 0;
    // TODO: actually implement combo/fallback keys?

    // NOTE: the problem is that this ONLY supports 1 key
	cvar_key_bind(
		const char* key,
		keybind_entry value,
		const char* comment,
		CVAR_T type,
		const char* file,
		int line);
    bool convert_string_to_event(const char* buffer, size_t size);
	NDSERR bool cvar_read(const char* buffer) override;
	std::string cvar_write() override;
};

extern cvar_key_bind cv_bind_forward;