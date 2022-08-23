#pragma once

#include "../global.h"
#include "font_manager.h"

#define STB_TEXTEDIT_CHARTYPE char32_t

//#define STB_TEXTEDIT_UNDOCHARCOUNT 10
//#define STB_TEXTEDIT_UNDOSTATECOUNT 3
#include "../3rdparty/stb_textedit.h"

#include <SDL2/SDL.h>

enum class TEXT_PROMPT_RESULT
{
	EAT,
	CONTINUE,
	ERROR
};
typedef uint8_t TEXTP_FLAG;
enum TEXT_PROMPT_FLAGS : TEXTP_FLAG
{
	TEXTP_NONE = 0,
	// word wrap based on the box width.
	// don't mix with single_line or y_scrollable
	// if this is disabled, wrapping could still happen.
	TEXTP_WORD_WRAP = (1 << 1),
	// don't mix with single_line
	TEXTP_Y_SCROLL = (1 << 2),
	// don't mix with word_wrap
	TEXTP_X_SCROLL = (1 << 3),
	TEXTP_READ_ONLY = (1 << 4),
	// don't mix with y_scrollable or word_wrap
	// this will disable line wrapping.
	TEXTP_SINGLE_LINE = (1 << 5),
	// draw a box around the text and scrollbar
	// without it the scrollbar thumb will still draw (if you have one)
	TEXTP_DRAW_BBOX = (1 << 6),
	// this will make all the text render
	// this will disable line wrapping.
	// if the text is outside the box, the mouse wont work.
	TEXTP_DISABLE_CULL = (1 << 7)
};

struct text_prompt_wrapper
{
	// this is a very poor structure for a text editor
	// a std::vector<std::string> would be better.
	// If I can't handle 100k character files,
	// I will probably need to switch to that.

	struct prompt_char
	{
		// char32_t is overkill, but stb_textedit doesn't like utf8
		// Also in hindsight, the downside of ucs32 is that
		// it's harder to limit the prompt text to a utf8 length...
		char32_t codepoint;
		float advance;
	};

	std::deque<prompt_char> text_data;

	// this is a tad bit large
	STB_TexteditState stb_state;

	font_sprite_batcher* batcher = NULL;

	// IME text that is displayed.
	std::string markedText;

#ifdef IME_TEXTEDIT_EXT
	int marked_cursor_begin = -1;
	int marked_cursor_end = -1;
#endif

	// caret (cursor) blinking timer
	TIMER_U blink_timer = 0;

	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	// if you have a scroll implemented, this is the top x and y offset,
	// this will offset and prune sprites from the draw_buffer,
	// but you still need to scissor out the edges of the scrollbox.
	float scroll_y = -1;
	float scroll_x = -1;

	// these are output variables from drawing
	// I use this for drawing the scrollbar.
	float scroll_w = -1;
	float scroll_h = -1;
	// this is used by both x and y scrollbars,
	// this is the offset that you clicked into the scroll thumb.
	float scroll_thumb_click_offset = -1;

	// I use this for filling a selection that only has a newline
	// and for a horrible hack where I need to replace the advance
	// of a space to signal that this space is word wrapping.
	float space_advance_cache = -1;

	// position the mouse dragged on the text after clicking
	float drag_x = -1;
	float drag_y = -1;

	float scrollbar_thickness = 20;
	float scrollbar_thumb_min_size = 20;

    // this is padding to help show the cursor at the right side of the screen
    float horizontal_padding = 30;

	std::array<uint8_t, 4> text_color{0, 0, 0, 255};
	std::array<uint8_t, 4> select_text_color{255, 255, 255, 255};
	std::array<uint8_t, 4> select_fill_color = RGBA8_PREM(80, 80, 255, 200);
	std::array<uint8_t, 4> unfocused_select_fill_color = RGBA8_PREM(80, 80, 80, 200);
	std::array<uint8_t, 4> scrollbar_color = RGBA8_PREM(80, 80, 80, 200);
	std::array<uint8_t, 4> caret_color{0, 0, 0, 255};
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	// I could use the bitfield trick to compress the size of bools,
	// but if I am only saving 4 bytes, it isn't worth it because it's ugly.

	// checks if the prompt is currently being edited.
	bool text_focus = false;
	// if you clicked into the text and hold down
	bool mouse_held = false;
	// signal to re-draw the text (into the batcher)
	bool update_buffer = true;
	// signals the draw function to draw a caret
	// this is not a flag
	bool draw_caret = false;
	// show the cursor in the next draw.
	bool scroll_to_cursor = false;
	// if you clicked on the scrollbar (if x_scrollable)
	bool y_scrollbar_held = false;
	bool x_scrollbar_held = false;

	TEXTP_FLAG flags = 0;

	// batcher needs to have a style set, and must be anchored TOP_LEFT
	// you need to bind the atlas before calling this.
	bool init(std::string_view contents, font_sprite_batcher* batcher_, TEXTP_FLAG flags_);

	// this will also check blink_timer and blink the cursor.
	// NOTE: but blink_timer should be in a logic() function...
	bool draw_requested();

	// you need to bind the atlas before calling this.
	bool replace_string(std::string_view contents);
	void clear_string();

	// this is an expensive operation
	std::string get_string() const;

	// you need to bind the atlas before calling this.
	TEXT_PROMPT_RESULT input(SDL_Event& e);

	// this draws into the batcher
	bool draw();

	void scroll_to_top()
	{
		ASSERT(y_scrollable());
		if(y_scrollable())
		{
			scroll_y = 0;
		}
		if(x_scrollable())
		{
			scroll_x = 0;
		}
		update_buffer = true;
	}
	void scroll_to_bottom()
	{
		ASSERT(y_scrollable());
		if(y_scrollable())
		{
			// the scroll will clamp itself.
			scroll_y = 99999999.f;
		}
		if(x_scrollable())
		{
			scroll_x = 0;
		}
		update_buffer = true;
	}

	// you should call draw after calling this.
	void set_bbox(float x, float y, float w, float h)
	{
		update_buffer = true;
		box_xmin = std::floor(x);
		box_xmax = std::floor(x + w);
		box_ymin = std::floor(y);
		box_ymax = std::floor(y + h);
	}
	void get_bbox(float* x, float* y, float* w, float* h) const
	{
		*x = box_xmin;
		*y = box_ymin;
		*w = box_xmax - box_xmin;
		*h = box_ymax - box_ymin;
	}

	void unfocus()
	{
		if(text_focus)
		{
			text_focus = false;
			update_buffer = true;
		}
	}

    void set_readonly(bool on)
	{
		if(on)
		{
			flags = flags | TEXTP_READ_ONLY;
		}
		else
		{
			flags = flags & ~TEXTP_READ_ONLY;
		}
	}

	// internal functions below

	// I usually hate getters, but using flag bits is too much.
	bool word_wrap() const
	{
		return (flags & TEXTP_WORD_WRAP) != 0;
	}
	bool y_scrollable() const
	{
		return (flags & TEXTP_Y_SCROLL) != 0;
	}
	bool x_scrollable() const
	{
		return (flags & TEXTP_X_SCROLL) != 0;
	}
	bool read_only() const
	{
		return (flags & TEXTP_READ_ONLY) != 0;
	}
	bool single_line() const
	{
		return (flags & TEXTP_SINGLE_LINE) != 0;
	}
	bool draw_bbox() const
	{
		return (flags & TEXTP_DRAW_BBOX) != 0;
	}
	bool cull_box() const
	{
		return (flags & TEXTP_DISABLE_CULL) == 0;
	}

	// scroll_w will not account for the padding for the scrollbar
	float get_scroll_width()
	{
		bool has_vertical = (y_scrollable() && scroll_h > (box_ymax - box_ymin));
		bool has_horizontal =
			(x_scrollable() &&
			 scroll_w + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding >
				 (box_xmax - box_xmin));
		has_vertical =
			(y_scrollable() &&
			 scroll_h + (has_horizontal ? scrollbar_thickness : 0) > (box_ymax - box_ymin));
		return scroll_w + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding;
	}
	// scroll_h will not account for the padding for the scrollbar
	float get_scroll_height()
	{
		bool has_horizontal =
			(x_scrollable() && scroll_w + horizontal_padding > (box_xmax - box_xmin));
		bool has_vertical =
			(y_scrollable() &&
			 scroll_h + (has_horizontal ? scrollbar_thickness : 0) > (box_ymax - box_ymin));
		has_horizontal =
			(x_scrollable() &&
			 scroll_w + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding >
				 (box_xmax - box_xmin));
		return scroll_h + (has_horizontal ? scrollbar_thickness : 0);
	}

	bool internal_draw_pretext();
	// TODO(dootsie): the offset was supposed to be from pretext to speed up scanning
	bool internal_draw_text(size_t offset, bool* caret_visible, float* caret_x, float* caret_y);
	bool internal_draw_marked(float x, float y);

	bool internal_scroll_y_inside(float mouse_x, float mouse_y);
	void internal_scroll_y_to(float mouse_y);

	bool internal_scroll_x_inside(float mouse_x, float mouse_y);
	void internal_scroll_x_to(float mouse_x);

	// functions internally used by stb_textedit
	void stb_layout_func(StbTexteditRow* row, int i);
	int stb_insert_chars(int index, const STB_TEXTEDIT_CHARTYPE* text, int n);
	void stb_delete_chars(int index, int n);
	float stb_get_width(int linestart, int index);
	STB_TEXTEDIT_CHARTYPE stb_get_char(int index);
};
