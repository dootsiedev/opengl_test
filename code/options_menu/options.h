#pragma once

#include "../global.h"

#include "../opengles2/opengl_stuff.h"
#include "../shaders/mono.h"
#include "../font/font_manager.h"
#include "../ui.h"

#include "options_select.h"
#include "options_video.h"
#include "options_controls.h"
#include "options_keybinds.h"
#ifdef AUDIO_SUPPORT
#include "options_audio.h"
#endif

#include <SDL2/SDL.h>

enum class OPTIONS_RESULT
{
	EAT,
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_state
{
	enum class MENU_FACTORY
	{
		MENU_SELECT,
		VIDEO,
		CONTROLS,
		KEYBINDS,
#ifdef AUDIO_SUPPORT
		AUDIO
#endif
	};
    // this isn't too great but it works.
	MENU_FACTORY current_state = MENU_FACTORY::KEYBINDS;
	options_select_state select;
	options_video_state video;
	options_controls_state controls;
	options_keybinds_state keybinds;
#ifdef AUDIO_SUPPORT
	options_audio_state audio;
#endif

	NDSERR bool init(
		font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTIONS_RESULT input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

    void resize_view();

	// call this when you need to unfocus, like for example if you press escape or something.
	void unfocus();
};