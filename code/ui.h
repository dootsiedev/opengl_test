#pragma once

#include "global.h"

#include "font/font_manager.h"

#include <SDL2/SDL.h>
#include <string>

// to understand set_event_leave & set_event_unfocus,
// you need to understand how I make elements focus and unfocus.
//
// -"input focus" means the element is activated and inputs are taken,
//      like for example a text prompt being focused (but not limited to just prompts).
// -"hover focus" is only for hovering over elements, like for example buttons.
// -"eating" an event means an event is taken and no other elements can use it.
//      for example set_event_unfocus, and set_event_leave will "eat" the event
//      by converting the event into something else.
// -elements can only set "input focus" internally from a LMB DOWN,
//      because it is assumed that any elements that are rendered above
//      would have eaten the LMB DOWN, and if the above element was focused,
//      it would unfocus itself because the LMB DOWN was outside the collision area.
// -if you want "input focus" for an element without LMB DOWN, like a key,
//      you would need to force ALL elements to unfocus first
//      (this is done by recursively calling input() with a dummy SDL_event
//      set with set_event_unfocus)

// set_event_leave
// converts the event to SDL_WINDOWEVENT_LEAVE
// I use this to remove "hover focus" from obscured elements,
// by eating the SDL_MOUSE_MOTION event.
// for example your mouse hovering over a button causing glow.
// NOTE: this only sets the type, so don't access any values.
// NOTE: if you suddenly present a menu above a button with "hover focus"
// like for example you were editing a map, and your mouse is
// hovering on a tile while you open up a menu with a hotkey,
// the "hover focus" will not change until you move the mouse...
// I need to figure out a clever way of handling that...
void set_event_leave(SDL_Event& e);

// set_event_unfocus
// converts the event to SDL_WINDOWEVENT_FOCUS_LOST
// I use this to remove "input focus" from elements,
// this has one job, which is to prevent an event
// from being eaten multiple times
// NOTE: this only sets the type, so don't access any values.
// this is used when you want to eat an input,
// and force other elements with text focus to lose focus.
// this does require all elements to handle this event.
void set_event_unfocus(SDL_Event& e);

// set_event_resize
// converts the event to SDL_WINDOWEVENT_SIZE_CHANGED
// you might need to call this if you present a menu
// that was hidden, and needs to explicitly resize.
// I like to think of this as a SDL_WINDOWEVENT_SHOWN
// except SDL_WINDOWEVENT_SHOWN != resize
// NOTE: this only sets the type, so don't access any values.
// use cv_screen_width and cv_screen_height!
void set_event_resize(SDL_Event& e);

// set_event_hidden
// converts the event to SDL_WINDOWEVENT_HIDDEN
// you might need to call this if you hide a menu
// this is basically set_event_leave + set_event_unfocus
// NOTE: this only sets the type, so don't access any values.
void set_event_hidden(SDL_Event& e);

enum class BUTTON_RESULT
{
	TRIGGER,
	CONTINUE,
	ERROR
};

struct button_color_state
{
	std::array<uint8_t, 4> bbox_color = {0, 0, 0, 255};
	std::array<uint8_t, 4> idle_fill_color = RGBA8_PREMULT(50, 50, 50, 200);
	std::array<uint8_t, 4> hot_fill_color = RGBA8_PREMULT(100, 100, 100, 200);
	std::array<uint8_t, 4> text_color = {255, 255, 255, 255};
	std::array<uint8_t, 4> text_outline_color = {0, 0, 0, 255};
	std::array<uint8_t, 4> disabled_text_color = {100, 100, 100, 255};
	std::array<uint8_t, 4> click_pop_fill_color = RGBA8_PREMULT(255, 255, 255, 200);
	float fade_speed = 4;
	bool text_outline = true;
};

struct mono_button_object
{
	button_color_state color_state;
	font_sprite_painter* font_painter = NULL;
	float fade = 0.f;
	// pos on the screen, x,y,w,h
	std::array<float, 4> button_rect{};
	bool hover_over = false;
    // use set_disabled instead of directly modifying this.
	bool disabled = false;
	// to make a click, you need to click down and up in the same area
	// if mouse_button_down = true, this does nothing.
	bool clicked_on = false;

    // make the button "pop" for a frame when you click
    bool display_click_frame = false;

    bool update_buffer = true;

	void init(font_sprite_painter* font_painter_, button_color_state* color_state_ = NULL)
	{
		ASSERT(font_painter_ != NULL);

		font_painter = font_painter_;
		if(color_state_ != NULL)
		{
			color_state = *color_state_;
		}
	}

    void set_disabled(bool on)
    {
        disabled = on;
        if(disabled)
        {
            hover_over = false;
            fade = 0;
        }
    }

	void set_rect(std::array<float, 4> pos_)
	{
		button_rect = pos_;
	}
	void set_rect(float x, float y, float w, float h)
	{
		button_rect = {x, y, w, h};
	}

    bool draw_requested() const{
        return update_buffer;
    }

	NDSERR BUTTON_RESULT input(SDL_Event& e);
	// update requires the buffer to be bound.
	void update(double delta_sec);
	NDSERR bool draw_buffer(const char* button_text, size_t button_text_len);
};

// renders the scroll bar
// only y scrollable because the design looks bad with an x axis.
// and I think using an x axis without text is silly
// (because with text, you can use drag selection to move the screen)
struct mono_y_scrollable_area
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
	// without the scrollbar
	float box_inner_xmax = -1;

	// the size of the contents in the scroll box
	float content_h = -1;

	float scrollbar_thickness = 20;
	float scrollbar_thumb_min_size = 20;
	// space between the scroll area and the scrollbar.
	float scrollbar_padding = 10;

	// the offset of the contents in the scroll box
	float scroll_y = 0;

	// this is the offset that you clicked into the scroll thumb.
	float scroll_thumb_click_offset = -1;

	bool y_scrollbar_held = false;

    bool update_buffer = true;

	void init(font_sprite_painter* font_painter_)
	{
		ASSERT(font_painter_ != NULL);
		font_painter = font_painter_;
	}

	// warning, this will only eat the scrollbar thumb being clicked,
	// the area set with resize_view will not eat click events (clicks go through)
	// so remember to catch backdrop clicks yourself.
	void input(SDL_Event& e);

	void draw_buffer();
    bool draw_requested() const{
        return update_buffer;
    }

	void scroll_to_top()
	{
		scroll_y = 0;
	}

	void unfocus();
	void resize_view(float xmin, float xmax, float ymin, float ymax);

	bool internal_scroll_y_inside(float mouse_x, float mouse_y);
	void internal_scroll_y_to(float mouse_y);
};

#if 0
// simple window that has a description text, and a list of buttons.
// mainly for a simple "yes or no" popup window.
struct simple_window_picker_state
{
	// this puts the text on the screen using a style and batcher.
	font_sprite_painter* font_painter = NULL;

	// this is just a list of buttons, I will just associate the button with the enum.
	struct select_entry
	{
		mono_button_object button;
		// -1 is reserved for errors.
		int result;
	};
	std::vector<select_entry> select_entries;

	std::string info_text;
	float menu_width = -1;

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

	// instead of being horizontal, list the buttons vertically.
	bool vertical_mode = false;

	// info is the message, width should be scaled by the pointsize of the font.
	void init(
		std::string info, float width, font_sprite_painter* font_painter_, GLuint vbo, GLuint vao);

	// add a button to the right, or down if vertical_mode is true.
	void add_button(std::string text, int result);

	NDSERR int input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	void resize_view();

	// call this when you need to unfocus, like for example if you press escape or something.
	void unfocus();
};
#endif

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

	// output value, values based on slider_min and slider_max
	double slider_value = 0;

    // these values clamp the slider value.
    double slider_min = 0;
    double slider_max = 1;

	float slider_thumb_size = 20;

	// this is the offset that you clicked into the scroll thumb.
	float slider_thumb_click_offset = -1;

	bool slider_held = false;

    bool update_buffer = true;

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
	bool draw_requested() const
	{
		return update_buffer;
	}

    // you need to use this because it sets draw_requested()
    void set_value(double value)
    {
        slider_value = value;
        update_buffer = true;
    }
    double get_value() const
    {
        return slider_value;
    }

	void unfocus();
	void resize_view(float xmin, float xmax, float ymin, float ymax);

	bool internal_slider_inside(float mouse_x, float mouse_y);
	void internal_move_to(float mouse_x);


	// get the value in a 0-1 range.
    double get_slider_normalized() const
    {
        return (slider_value - slider_min) / (slider_max - slider_min);
    }
};