#pragma once

#include "../global.h"

#include "../opengles2/opengl_stuff.h"
#include "../shaders/mono.h"
#include "../font/font_manager.h"
#include "../keybind.h"
#include "../ui.h"

#include <SDL2/SDL.h>

enum class OPTIONS_KEYBINDS_RESULT
{
	EAT,
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_keybinds_state
{
	// font_style_interface* options_font = NULL;
	// mono_2d_batcher* options_batcher = NULL;

	// this puts the text on the screen using a style and batcher.
	font_sprite_painter font_painter;

    mono_y_scrollable_area scroll_state;

	struct keybind_entry
	{
		explicit keybind_entry(cvar_key_bind& keybind_)
		: keybind(keybind_)
		{
		}
		cvar_key_bind& keybind;
		mono_button_object button;
	};
	std::vector<keybind_entry> buttons;

	// maybe add a X button?
	mono_button_object revert_button;
	mono_button_object ok_button;
	mono_button_object defaults_button;

	struct edit_history
	{
		explicit edit_history(keybind_state value_, keybind_entry& slot_)
		: value(value_)
		, slot(slot_)
		{
		}
		keybind_state value;
		keybind_entry& slot;
	};
	std::vector<edit_history> history;

	// the buffer that contains the menu rects and text
	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

	keybind_entry* requested_button = NULL;

	// area that the bottom buttons go
	// based on the size of the font.
	// used for getting the size of the scrollbox.
	float footer_height = -1;
	// added size to the lineskip for the button size.
	float font_padding = 4;
	// padding between elements (buttons, scrollbar, etc)
	float element_padding = 10;

    // the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	NDSERR bool init(
		font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTIONS_KEYBINDS_RESULT input(SDL_Event& e);

	NDSERR bool draw_base();
	NDSERR bool draw_scroll();

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

    void resize_view();

	// call this when you need to unfocus, like for example if you press escape or something.
	void unfocus();
};