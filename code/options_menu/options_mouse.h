#pragma once

#include "../ui.h"
#include "../font/text_prompt.h"

// TODO: move this into ui.h
// a slider that takes a double from 0-1.
struct mono_normalized_slider_object
{
	// I don't need a font, but I use the lineskip for the scroll speed.............
	// also contains the batcher, and white_uv I need.
	// YOU CAN NOT USE THE FONT BECAUSE I DONT BIND THE ATLAS!
	font_sprite_painter* font_painter = NULL;

	// fill color of the thumb
	std::array<uint8_t, 4> scrollbar_color = RGBA8_PREMULT(80, 80, 80, 200);
	// the outlines
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
	// optional fill
	std::array<uint8_t, 4> fill_color{0, 0, 0, 255};

	// screen coords of the scrollbox and scroll bar
	// to get the area without the scrollbar, use box_inner_xmax
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	// output value, from 0-1
	double slider_value = 0;

	float slider_thumb_size = 20;

	// this is the offset that you clicked into the scroll thumb.
	float slider_thumb_click_offset = -1;

	bool slider_held = false;

	// initial_value is a value from 0-1.
	void init(font_sprite_painter* font_painter_, double initial_value)
	{
		ASSERT(font_painter_ != NULL);
		slider_value = initial_value;
		font_painter = font_painter_;
	}

    // this does not return an error!!!
    // this returns true if the value changed!
	bool input(SDL_Event& e);

	void draw_buffer();

	void unfocus();
	void resize_view(float xmin, float xmax, float ymin, float ymax);

	bool internal_slider_inside(float mouse_x, float mouse_y);
	void internal_move_to(float mouse_x);
};

enum class OPTIONS_MOUSE_RESULT
{
	CLOSE,
	CONTINUE,
	ERROR
};
struct options_mouse_state
{
	// this puts the text on the screen using a style and batcher.
	font_sprite_painter* font_painter = NULL;

	// I am sure this is the worst possible way of doing this,
	// and it would be quite easy to just create a list of abstract
	// option elements that are tied to generic cvars.
	// but I like this because everything is exposed,
	// this means it's very difficult to add new options,
	// but easy to add in special case UI elements (if I had them)
	enum
	{
		// yikes.
		OPTION_COUNT = 2
	};

	std::string invert_text;
	mono_button_object invert_button;
    // 0 = false,  1 = true, -1 = not modified
    int previous_invert_value = -1;

	std::string mouse_sensitivity_text;
    text_prompt_wrapper mouse_sensitivity_prompt;
	mono_normalized_slider_object mouse_sensitivity_slider;
    double previous_mouse_sensitivity_value = NAN;

    //footer buttons
	mono_button_object revert_button;
	mono_button_object ok_button;
	mono_button_object defaults_button;

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

	NDSERR bool init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao);

	NDSERR OPTIONS_MOUSE_RESULT input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

    NDSERR bool draw_text();

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	void resize_view();

    NDSERR bool undo_history();
    void clear_history();
    NDSERR bool set_defaults();
    void close();

};