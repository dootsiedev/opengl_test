#pragma once

#include "global.h"

#include "font/font_manager.h"

#include <SDL2/SDL.h>
#include <string>

// to understand set_event_leave & set_event_unfocus,
// you need to understand how I make elements focus and unfocus.
// -elements can only set "input focus" internally from a LMB DOWN, 
// because it is assumed that any elements that are rendered above
// would have eaten the LMB DOWN (set_event_unfocus), and
// if the above element was focused, it would unfocus itself because
// the LMB DOWN was outside the collision area (otherwise it would eat it).
// if a missed LMB DOWN collision didn't unfocus that would be bad behavior.
// -if you want "input focus" for an element without LMB DOWN, 
// you would need to force ALL elements to unfocus first
// (this is done by recursively calling input() with a dummy SDL_event
// set with set_event_unfocus)

// set the event to SDL_WINDOWEVENT_LEAVE
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

// set the event to SDL_WINDOWEVENT_FOCUS_LOST
// I use this to remove "input focus" from elements,
// this has one job, which is to prevent an event 
// from being eaten multiple times
// NOTE: this only sets the type, so don't access any values.
// this is used when you want to eat an input,
// and force other elements with text focus to lose focus.
// this does require all elements to handle this event.
void set_event_unfocus(SDL_Event& e);


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
    // to make a click, you need to click down and up in the same area
    // if mouse_button_down = true, this does nothing.
    bool clicked_on = false;

	void init(font_sprite_painter* font_painter_, button_color_state* color_state_ = NULL)
	{
		ASSERT(font_painter_ != NULL);

		font_painter = font_painter_;
		if(color_state_ != NULL)
		{
			color_state = *color_state_;
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

	NDSERR BUTTON_RESULT input(SDL_Event& e);
	// update requires the buffer to be bound.
	NDSERR bool update(double delta_sec);
	NDSERR bool draw_buffer();
	void unfocus();
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


	void init(font_sprite_painter* font_painter_)
    {
        ASSERT(font_painter_ != NULL);
        font_painter = font_painter_;
    }

    void input(SDL_Event& e);

	void draw_buffer();

    void unfocus()
    {
        y_scrollbar_held = false;
	    scroll_thumb_click_offset = -1;
    }
    void resize_view(float xmin,float xmax,float ymin,float ymax)
    {
        box_xmin = xmin;
        box_xmax = xmax;
        box_ymin = ymin;
        box_ymax = ymax;
        // probably should use content_h > (ymax-ymin), but this feels more stable
        box_inner_xmax = xmax - scrollbar_thickness - scrollbar_padding;
        // clamp the scroll (when the screen resizes)
        scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
    }

	bool internal_scroll_y_inside(float mouse_x, float mouse_y);
	void internal_scroll_y_to(float mouse_y);

};