#pragma once

#include "opengles2/opengl_stuff.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"
#include "keybind.h"

#include <SDL2/SDL.h>

#include <string>

enum class BUTTON_RESULT
{
	TRIGGER,
	CONTINUE,
	ERROR
};

// TODO: button should be put somehere else where more menus can access
struct button_color_state
{
	std::array<uint8_t, 4> bbox_color = {0, 0, 0, 255};
	std::array<uint8_t, 4> idle_fill_color = RGBA8_PREMULT(50, 50, 50, 200);
	std::array<uint8_t, 4> hot_fill_color = RGBA8_PREMULT(100, 100, 100, 200);
	std::array<uint8_t, 4> text_color = {255, 255, 255, 255};
	std::array<uint8_t, 4> text_outline_color = {0, 0, 0, 255};
	std::array<uint8_t, 4> disabled_text_color = {100, 100, 100, 255};
	float fade_speed = 4;
    bool show_outline = true;
};

struct mono_button_object
{
	button_color_state color_state;
	font_sprite_painter* font_painter = NULL;
	std::string text;
	float fade = 0.f;
	// pos on the screen, x,y,w,h
	std::array<float, 4> button_rect{};
	bool hover_over = false;
	bool disabled = false;
    // activate the button on button down instead of up..
    bool mouse_button_down = false;
    // to make a click, you need to click down and up in the same area
    // if mouse_button_down = true, this does nothing.
    bool clicked_on = false;

	void init(font_sprite_painter* font_painter_, button_color_state* color_state_ = NULL);

	void set_rect(std::array<float, 4> pos_)
	{
		button_rect = pos_;
	}
	void set_rect(float x, float y, float w, float h)
	{
		button_rect = {x, y, w, h};
	}

	NDSERR BUTTON_RESULT input(SDL_Event& e);
	// update requires the buffer to be bound.
	NDSERR bool update(double delta_sec);
	NDSERR bool draw_buffer();
	void unfocus();
};

enum class OPTION_MENU_RESULT
{
	EAT,
	CLOSE,
	CONTINUE,
	ERROR
};

struct option_menu_state
{
	// font_style_interface* options_font = NULL;
	// mono_2d_batcher* options_batcher = NULL;

	// this puts the text on the screen using a style and batcher.
	font_sprite_painter font_painter;

	struct keybind_entry
	{
		explicit keybind_entry(cvar_key_bind& keybind_)
		: keybind(keybind_)
		{
		}
		cvar_key_bind& keybind;
		mono_button_object button;
	};
	button_color_state button_color_config;
	std::vector<keybind_entry> buttons;

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

	// maybe add a X button?
	mono_button_object revert_button;
	mono_button_object ok_button;
	mono_button_object defaults_button;

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

	// probably should have some sort of system to make this not be copy pasted from prompt code...
	bool y_scrollbar_held = false;

	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	// the offset of the scroll box
	float scroll_y = 0;

	// the height of the contents in the scroll box
	float scroll_h = -1;

	// this is the offset that you clicked into the scroll thumb.
	float scroll_thumb_click_offset = -1;

	// position the mouse dragged on the text after clicking
	float scroll_drag_y = -1;

	float scrollbar_thickness = 20;
	float scrollbar_thumb_min_size = 20;

	NDSERR bool init(
		font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTION_MENU_RESULT input(SDL_Event& e);

	NDSERR bool draw_base();
	NDSERR bool draw_scroll();

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	// call this when you need to unfocus, like for example if you press escape or something.
	void unfocus();

	void internal_scroll_y_to(float mouse_y);
	bool internal_scroll_y_inside(float mouse_x, float mouse_y);

	// x,y,w,h
	std::array<float, 4> internal_get_scrollbox_view() const;
};