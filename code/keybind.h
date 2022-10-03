#pragma once

#include "cvar.h"

#include <SDL2/SDL.h>

// keyboard
#define REGISTER_CVAR_KEY_BIND_KEY(key, value, allow_mouse, comment, type) \
	cvar_key_bind key(                                                \
		#key, {KEYBIND_T::KEY, 0, value, 0}, allow_mouse, comment, type, __FILE__, __LINE__)
// mouse
#define REGISTER_CVAR_KEY_BIND_MOUSE(key, value, allow_mouse, comment, type) \
	cvar_key_bind key(                                                  \
		#key, {KEYBIND_T::MOUSE, value, 0, 0}, allow_mouse, comment, type, __FILE__, __LINE__)
// this is used for fullscreen alt + enter.
#define REGISTER_CVAR_KEY_BIND_KEY_AND_MOD(key, value, mod, allow_mouse, comment, type) \
	cvar_key_bind key(                                                             \
		#key, {KEYBIND_T::KEY, 0, value, mod}, allow_mouse, comment, type, __FILE__, __LINE__)
// unbound key.
#define REGISTER_CVAR_KEY_BIND_NONE(key, allow_mouse, comment, type) \
	cvar_key_bind key(#key, {KEYBIND_T::NONE, 0, 0, 0}, allow_mouse, comment, type, __FILE__, __LINE__)

// used for registry
enum class KEYBIND_T : uint8_t
{
    NONE,
	KEY,
	MOUSE
	// TODO: scroll wheel, maybe controller?
};
// used for registry
struct keybind_state
{
	// union-like, not worth making into a union.
    // and very unfortunately, I want to use std::variant,
    // but these types are ambiguous (code is horrible)
	KEYBIND_T type = KEYBIND_T::NONE;
    // SDL_BUTTON_LEFT or SDL_BUTTON_RIGHT, MIDDLE
	uint8_t mouse_button;
    // SDLK_... keybord keys
	SDL_Keycode key;
    // KMOD_...SHIFT/CTRL/ALT
    Uint16 mod;
};

//the keybind enums are used for compare_sdl_event flags and return
typedef uint8_t keybind_compare_type;
enum : keybind_compare_type
{
	// KEYBIND_NO_INPUT is only used for returns!
	KEYBIND_NULL = 0,
	KEYBIND_BUTTON_DOWN = (1 << 1),
	KEYBIND_BUTTON_UP = (1 << 2),
    // KEYBIND_REPEAT will make keys repeat when held down (timed by SDL)
    // and the return will OR this into the result if key is repeating 
	KEYBIND_REPEAT = (1 << 3)
};

// the cvar is a string formatted like this:
// "SDLK_a" means it uses "a" on the keyboard, 
// and the string is copied from SDL's enum for SDL_Keycode
// if you want the key to only be triggered with a combination of keys
// you can use ctrl,alt,shift modifiers: 
// "KMOD_SHIFT;SDLK_a" means you must hold shift AND "a",
//  and shift must be held before pressing A (pressing shift won't trigger it).
// note that if you use modifiers, it won't work for KEYBIND_BUTTON_UP
// (because for movement, if you release the modifier before releasing the key, 
//  KEYBIND_BUTTON_UP will not be detected, I could fix this with flags...)
// note that if you combine modifiers, it will only require one modifier for the input
// (this is because SDL KMOD_SHIFT is a combination of KMOD_LSHIFT and KMOD_RSHIFT)
class cvar_key_bind : public V_cvar
{
public:

	// SDL_Event is just a very convenient union,
	// I mainly just use keyboard and mouse
	// but this could support a controller as well.
	keybind_state key_binds;

    // if I set fullscreen to LMB, 
    // I cannot undo the change because LMB would always be eaten.
    // so you probably don't want to allow the mouse to be an input.
    bool allow_mouse = true;

	// NOTE: the problem is that this ONLY supports 1 key
	// Note that I really want to use a string for the registry,
	// but the problem is that if an error occurrs, it can't be caught in serr
	// because of Static Initialization Order Fiasco,
	// so I need some way to make cvar_read NOT use serr?
	cvar_key_bind(
		const char* key,
		keybind_state value,
        bool allow_mouse_,
		const char* comment,
		CVAR_T type,
		const char* file,
		int line);
	NDSERR bool cvar_read(const char* buffer) override;
	std::string cvar_write() override;

    std::string keybind_to_string(keybind_state& in);
	// returns true if the event is eaten.
	bool bind_sdl_event(keybind_state& value, SDL_Event& e);
	// returns the flag that was eaten, if you use KEYBIND_REPEAT, it will be OR'd into the return.
	keybind_compare_type compare_sdl_event(SDL_Event& e, keybind_compare_type flags);
	// internal use
	bool convert_string_to_event(const char* buffer, size_t size);
};