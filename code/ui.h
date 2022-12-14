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

/// TODO: set_event_leave is replaced by set_motion_event_clipped
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
/// void set_event_leave(SDL_Event& e);

// set_event_unfocus
// makes more sense if it was set_event_eat
// converts the event to SDL_WINDOWEVENT_FOCUS_LOST
// I use this to remove "input focus" from elements,
// this has two jobs, prevent an event from being eaten (used) multiple times,
// and remove keyboard focus from elements that are processed AFTER this element.
// (for elements before THIS element, non colliding LMB DOWN will remove focus)
// NOTE: this only sets the type, so don't access any values.
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

// this is used in situations you would use set_event_leave
// but you don't want the event to be 100% eaten,
// like for mouse motion events, if you were dragging a slider,
// or text selection, you don't want the motion to be eaten,
// even if it's technically outside the element.
// but you still need to check if the event was clipped to remove "hover focus"
// NOTE: this must be a button or motion event,
// I do this for wheel events, you must set the type, and set the e.motion.x and y values.
void set_mouse_event_clipped(SDL_Event& e);

enum : Uint32
{
	CLIPPED_WINDOW_ID = (1 << 30),
	TEXT_INPUT_STOLEN_WINDOW_ID = (1 << 29)
};

// with scrollable areas, if the mouse is hovering over a clipped area,
// I will set the event for all elements inside the scroll area
// to activate is_mouse_event_clipped
// for SDL_MOUSEBUTTONUP and SDL_MOUSEBUTTONDOWN and SDL_MOUSEMOTION
// it's a ugly hack but you need to handle this special case.
bool is_mouse_event_clipped(SDL_Event& e);

// if you click into a prompt (SDL_StartTextInput),
// it will could cause another prompt down the line to unfocus (SDL_StopTextInput),
// and then you have a prompt that is focused, but can't take input.
bool is_unfocus_event_text_input_stolen(SDL_Event& e);

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
	std::array<uint8_t, 4> click_pop_fill_color = RGBA8_PREMULT(200, 200, 200, 255);
	float fade_speed = 4;
	bool text_outline = true;
};

struct mono_button_object
{
	button_color_state color_state;
	font_sprite_painter* font_painter = NULL;
	float fade = 0.f;
	// make the button "pop" for a frame when you click
	float pop_effect = 0.f;
	// pos on the screen, x,y,w,h
	std::array<float, 4> button_rect{};
	bool hover_over = false;
	// use set_disabled instead of directly modifying this.
	bool disabled = false;
	// to make a click, you need to click down and up in the same area
	// if mouse_button_down = true, this does nothing.
	bool clicked_on = false;

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
		update_buffer = true;
	}
	void set_rect(float x, float y, float w, float h)
	{
		button_rect = {x, y, w, h};
		update_buffer = true;
	}

	bool draw_requested() const
	{
		return update_buffer;
	}

	NDSERR BUTTON_RESULT input(SDL_Event& e);
	// update requires the buffer to be bound.
	void update(double delta_sec);
	NDSERR bool draw_buffer(const char* button_text, size_t button_text_len);
};

enum class SCROLLABLE_AREA_RETURN
{
	CONTINUE,
	MODIFIED
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
	SCROLLABLE_AREA_RETURN input(SDL_Event& e);

	void draw_buffer();
	bool draw_requested() const
	{
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
	void init(
		font_sprite_painter* font_painter_,
		double initial_value,
		double slider_min_,
		double slider_max_);

	// this does not return an error!!!
	// this returns true if the value changed!
	// TODO: probably could add in a UNFOCUS enum if I didn't want the realtime modification.
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