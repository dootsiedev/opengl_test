#pragma once

#include "../global.h"

#include "../ui.h"

enum class OPTIONS_SELECT_RESULT
{
	OPEN_VIDEO,
	OPEN_MOUSE,
	OPEN_KEYBINDS,
#ifdef AUDIO_SUPPORT
	OPEN_AUDIO,
#endif
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_select_state
{
	// this puts the text on the screen using a style and batcher.
	font_sprite_painter* font_painter = NULL;

	// this is just a list of buttons, I will just associate the button with the enum.
	struct select_entry
	{
		mono_button_object button;
        std::string text;
		OPTIONS_SELECT_RESULT result;
	};
	std::vector<select_entry> select_entries;

	// the buffer that contains the menu rects and text
	// this is NOT owned by this state
	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

	// added size to the lineskip for the button size.
	float font_padding = 4;
	// padding between elements (buttons, scrollbar, etc)
	float element_padding = 10;

	// the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	void init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao);

	NDSERR OPTIONS_SELECT_RESULT input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	void resize_view();
};