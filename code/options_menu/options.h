#pragma once

#include "../global.h"

#include "../opengles2/opengl_stuff.h"
#include "../shaders/mono.h"
#include "../font/font_manager.h"
#include "../ui.h"

#include "options_select.h"
#include "options_video.h"
#include "options_controls.h"
#ifdef AUDIO_SUPPORT
#include "options_audio.h"
#endif

#include <SDL2/SDL.h>

enum class OPTIONS_RESULT
{
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_state
{
	// this puts the text on the screen using a style and batcher.
	font_sprite_painter font_painter;

	enum class MENU_FACTORY
	{
		MENU_SELECT,
		VIDEO,
		CONTROLS,
		//MOUSE,
#ifdef AUDIO_SUPPORT
		AUDIO
#endif
	};
	// this isn't too great but it works.
	MENU_FACTORY current_state = MENU_FACTORY::MENU_SELECT;
	options_select_state select;
	options_video_state video;
	options_controls_state controls;
#ifdef AUDIO_SUPPORT
	options_audio_state audio;
#endif

	// the buffer that contains the menu rects and text
	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

	NDSERR bool init(
		font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTIONS_RESULT input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	void internal_refresh();
};