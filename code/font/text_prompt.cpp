#include "../global.h"
#include "../app.h" // for the scroll cvar
#include "text_prompt.h"
#include "utf8_stuff.h"
#include "../ui.h"

// TODO(dootsie): add in double click selection?
// TODO(dootsie): add in size limited option (in bytes)
// TODO(dootsie): the text is too close to the bbox, should add in a padding.
// TODO(dootsie): add in a MODIFY result to have a way to check if the text was modified
// TODO(dootsie): add a font_padding so the bbox can be not so tight because the outline can clip.
//          but one problem is that single_line prompts use lineskip() for height,
//          maybe make get_single_line_height() and ignore the value set for the height.
// TODO(dootsie): make the mouse change it's icon to the I beam when hovering over the prompt?

#define STB_TEXTEDIT_KEYTYPE SDL_Keycode
#define STB_TEXTEDIT_STRING text_prompt_wrapper
// using HUGE_VALF is kind of weird (and stupid),
// but it's basically NAN except you can compare HUGE_VALF == HUGE_VALF or INFINITY
#define STB_TEXTEDIT_GETWIDTH_NEWLINE HUGE_VALF
#define STB_TEXTEDIT_NEWLINE '\n'

// STB_TEXTEDIT_IS_SPACE is used to implement ctrl+arrow, U+3000 is a CJK space
#define STB_TEXTEDIT_IS_SPACE(ch) ((ch) == ' ' || (ch) == '\t' || (ch) == 0x3000)
// all characters are pasted through SDL's TEXTEDIT events
// except for special characters like tab or newline,
// so I don't need to worry about making sure keycodes don't mix with unicode.
#define STB_TEXTEDIT_KEYTOTEXT(key) (key)

#define STB_TEXTEDIT_STRINGLEN(obj) static_cast<int>((obj)->text_data.size())

#define STB_TEXTEDIT_LAYOUTROW(row, obj, n) (obj)->stb_layout_func((row), (n))
#define STB_TEXTEDIT_DELETECHARS(obj, i, n) (obj)->stb_delete_chars((i), (n))
#define STB_TEXTEDIT_INSERTCHARS(obj, i, c, n) (obj)->stb_insert_chars((i), (c), (n))
#define STB_TEXTEDIT_GETWIDTH(obj, n, i) (obj)->stb_get_width((n), (i))
#define STB_TEXTEDIT_GETCHAR(obj, i) (obj)->stb_get_char((i))

// and I abuse the 29th and 28th bit, hopefully SDL doesn't use it for anything.
#define STB_TEXTEDIT_K_SHIFT (1 << 29)
#define STB_TEXTEDIT_K_CONTROL (1 << 28)

#define STB_TEXTEDIT_K_LEFT SDLK_LEFT
#define STB_TEXTEDIT_K_RIGHT SDLK_RIGHT
#define STB_TEXTEDIT_K_UP SDLK_UP
#define STB_TEXTEDIT_K_DOWN SDLK_DOWN
#define STB_TEXTEDIT_K_BACKSPACE SDLK_BACKSPACE
#define STB_TEXTEDIT_K_UNDO (SDLK_z | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_REDO (SDLK_y | STB_TEXTEDIT_K_CONTROL)

#define STB_TEXTEDIT_K_LINESTART SDLK_HOME
#define STB_TEXTEDIT_K_LINEEND SDLK_END
#define STB_TEXTEDIT_K_TEXTSTART (STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_TEXTEND (STB_TEXTEDIT_K_LINEEND | STB_TEXTEDIT_K_CONTROL)
// this is not actually used
#define STB_TEXTEDIT_K_DELETE SDLK_DELETE
#define STB_TEXTEDIT_K_INSERT SDLK_INSERT
#define STB_TEXTEDIT_K_WORDLEFT (STB_TEXTEDIT_K_LEFT | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_WORDRIGHT (STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_CONTROL)
// this is not actually used
#define STB_TEXTEDIT_K_PGUP SDLK_PAGEUP
#define STB_TEXTEDIT_K_PGDOWN SDLK_PAGEDOWN

#define STB_TEXTEDIT_IMPLEMENTATION
#include "../3rdparty/stb_textedit.h"

bool text_prompt_wrapper::init(
	std::string_view contents,
	mono_2d_batcher* batcher_,
	font_style_interface* font_,
	TEXTP_FLAG flags_)
{
	ASSERT(batcher_ != NULL);
	ASSERT(font_ != NULL);

	state.init(batcher_, font_);
	flags = flags_;

	// does this need to be zero'd?
	stb_textedit_initialize_state(&stb_state, single_line() ? 1 : 0);

	replace_string(contents);

	ASSERT(!(word_wrap() && single_line()) && "single line can't be wrapped");
	ASSERT(!(y_scrollable() && single_line()) && "single line can't be scrollable");
	ASSERT(!(word_wrap() && x_scrollable()) && "can't have both wordwrap and x scrolling");

	// space_advance_cache = painter->font->GetAdvance(' ');

	// implicitly set the space cache value.
	if(!set_scale(state.font_scale))
    {
        return false;
    }

	return true;
}

bool text_prompt_wrapper::set_scale(float font_scale)
{
	state.font_scale = font_scale;
	// fix the cached size of the space.
	switch(state.font->get_advance(' ', &space_advance_cache, state.font_scale))
	{
	case FONT_BASIC_RESULT::SUCCESS: break;
	case FONT_BASIC_RESULT::NOT_FOUND:
		serrf("%s: space not found???\n", state.font->get_name());
		return false;
	case FONT_BASIC_RESULT::ERROR: return false;
	}
	return true;
}

void text_prompt_wrapper::replace_string(std::string_view contents, bool clear_history)
{
	update_buffer = true;

	std::u32string wstr;
	wstr.reserve(contents.size());
	std::string_view::iterator str_cur = contents.begin();
	std::string_view::iterator str_end = contents.end();
	while(str_cur != str_end)
	{
		uint32_t codepoint;
		utf8::internal::utf_error err_code =
			utf8::internal::validate_next(str_cur, str_end, codepoint);
		if(err_code != utf8::internal::UTF8_OK)
		{
			slogf("info: %s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			break;
		}
		wstr.push_back(codepoint);
	}

	if(clear_history)
	{
		// I clear the state because stb_textedit_paste will create 2 undo's
		// otherwise I would only reset the state if this was readonly() to save memory (if it was
		// dynamic).
		stb_textedit_clear_state(&stb_state, single_line() ? 1 : 0);
		text_data.clear();
	}
	else
	{
		stb_state.select_start = 0;
		stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
		// stb_state.cursor = STB_TEXTEDIT_STRINGLEN(this);
	}

	// avoid read only error.
	TEXTP_FLAG was_readonly = flags;
	flags = flags & ~TEXTP_READ_ONLY;
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	// int ret =
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	stb_textedit_paste(this, &stb_state, wstr.data(), wstr.size());
	flags = was_readonly;

	// why not set the cursor to the end.
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	stb_state.cursor = text_data.size();

	/*if(ret == 0)
	{
		batcher = NULL;
		serrf(
			"%s failed to paste clipboard: `%*.s`\n",
			__func__,
			static_cast<int>(contents.size()),
			contents.data());
		return false;
	}*/

	// return true;
}
void text_prompt_wrapper::clear_string()
{
	update_buffer = true;

	// TODO(dootsie): I could implement this to be undo'able
	// I would make this function use a bool to set wether or not you want keep undo state,
	// and I would need to manually create a undo record, because paste would create 2.
	stb_textedit_clear_state(&stb_state, single_line() ? 1 : 0);
	text_data.clear();

	// stb_state.select_start = STB_TEXTEDIT_STRINGLEN(this);
	// stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
	// stb_state.cursor = STB_TEXTEDIT_STRINGLEN(this);
}

std::string text_prompt_wrapper::get_string() const
{
	std::string out;
	for(const prompt_char& c : text_data)
	{
		if(!cpputf_append_string(out, c.codepoint))
		{
			slogf("info: invalid codepoint from prompt: U+%X\n", c.codepoint);
		}
	}
	return out;
}

void text_prompt_wrapper::update(double)
{
	// if you tab out of the window text input can deactivate without sending ""
	// also if you suddenly set the text to be read only, clear it.
	if(!markedText.empty() && (SDL_IsTextInputActive() == SDL_FALSE || read_only()))
	{
		markedText.clear();
		update_buffer = true;
	}

	// this will secretly check the caret.
	// technically I should draw the cursor,
	// but it doesn't matter, because if lag was noticable,
	// you will still see it while editing.
	if(text_focus && !read_only())
	{
		TIMER_U now = timer_now();
		if(timer_delta_ms(blink_timer, now) > 1000)
		{
			blink_timer = now;
		}
		if(timer_delta_ms(blink_timer, now) < 500)
		{
			if(!draw_caret)
			{
				// TODO(dootsie): (performance) the caret forces the text to be re-drawn,
				// with address sanitizer this can be felt for large files.
				update_buffer = true;
				draw_caret = true;
			}
		}
		else
		{
			if(draw_caret)
			{
				update_buffer = true;
				draw_caret = false;
			}
		}
	}
	else
	{
		if(draw_caret)
		{
			update_buffer = true;
			draw_caret = false;
		}
	}
}

bool text_prompt_wrapper::draw()
{
	float lineskip = get_lineskip();

	// we don't need to draw again.
	update_buffer = false;

	if(mouse_held)
	{
		stb_textedit_drag(this, &stb_state, drag_x, drag_y);
	}

	if(!internal_draw_pretext())
	{
		return false;
	}

	float caret_x = -1;
	float caret_y = -1;
	bool caret_visible = false;
	if(!internal_draw_text(0, &caret_visible, &caret_x, &caret_y))
	{
		return false;
	}

// SDL_IsTextInputShown is available on the same verison TEXTEDIT_EXT is supported.
#ifdef IME_TEXTEDIT_EXT
	// SDL_IsTextInputShown doesn't work on linux mint SDL2 2.0.23 with Mozc IME
	// when the IME prompt is empty. I probably NEED to call SDL_StartTextInput
	// AFTER I set SDL_SetTextInputRect, which is tedious...
	if(markedText.empty() && SDL_IsTextInputShown() == SDL_FALSE)
#else
	if(markedText.empty())
#endif
	{
		// draw the cursor
		if(draw_caret && caret_visible)
		{
			if(!cull_box() || (box_ymax >= caret_y && box_ymin <= caret_y + lineskip &&
							   box_xmax >= caret_x && box_xmin <= caret_x + 2))
			{
				ASSERT(text_focus);
				ASSERT(!read_only());
				std::array<float, 4> caret_pos{caret_x, caret_y, caret_x + 2, caret_y + lineskip};
				state.batcher->draw_rect(
					caret_pos, state.font->get_font_atlas()->white_uv, caret_color);
			}
		}
	}
	else if(!internal_draw_marked(caret_x, caret_y))
	{
		return false;
	}

	// at the end because when I draw a backdrop, I don't want the bbox to be under it
	internal_draw_widgets();

	return true;
}

void text_prompt_wrapper::internal_draw_widgets()
{
	auto white_uv = state.font->get_font_atlas()->white_uv;
	// draw the bbox
	if(draw_bbox())
	{
		state.batcher->draw_rect(
			{box_xmin, box_ymin, box_xmin + 1, box_ymax}, white_uv, bbox_color);
		state.batcher->draw_rect(
			{box_xmin, box_ymin, box_xmax, box_ymin + 1}, white_uv, bbox_color);
		state.batcher->draw_rect(
			{box_xmax - 1, box_ymin, box_xmax, box_ymax}, white_uv, bbox_color);
		state.batcher->draw_rect(
			{box_xmin, box_ymax - 1, box_xmax, box_ymax}, white_uv, bbox_color);

		// draw the scroll bar bbox
		if(!single_line())
		{
			bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
			bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
			if(has_vertical)
			{
				float scrollbar_max_height =
					(box_ymax - box_ymin) - (has_horizontal ? scrollbar_thickness - 1 : 0.f);
				float scrollbar_xmin = box_xmax - scrollbar_thickness;
				float scrollbar_xmax = box_xmax;
				float scrollbar_ymin = box_ymin;
				float scrollbar_ymax = box_ymin + scrollbar_max_height;

				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymin, scrollbar_xmin + 1, scrollbar_ymax},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymin, scrollbar_xmax, scrollbar_ymin + 1},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmax - 1, scrollbar_ymin, scrollbar_xmax, scrollbar_ymax},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymax - 1, scrollbar_xmax, scrollbar_ymax},
					white_uv,
					bbox_color);
			}
			if(has_horizontal)
			{
				float scrollbar_max_width =
					(box_xmax - box_xmin) - (has_vertical ? scrollbar_thickness - 1 : 0.f);
				float scrollbar_xmin = box_xmin;
				float scrollbar_xmax = box_xmin + scrollbar_max_width;
				float scrollbar_ymin = box_ymax - scrollbar_thickness;
				float scrollbar_ymax = box_ymax;

				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymin, scrollbar_xmin + 1, scrollbar_ymax},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymin, scrollbar_xmax, scrollbar_ymin + 1},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmax - 1, scrollbar_ymin, scrollbar_xmax, scrollbar_ymax},
					white_uv,
					bbox_color);
				state.batcher->draw_rect(
					{scrollbar_xmin, scrollbar_ymax - 1, scrollbar_xmax, scrollbar_ymax},
					white_uv,
					bbox_color);
			}
		}
	}

	if(!single_line())
	{
		// draw the thumbs
		bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
		bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
		if(has_vertical)
		{
			float scrollbar_max_height =
				(box_ymax - box_ymin) - (has_horizontal ? scrollbar_thickness - 1 : 0.f);
			float thumb_height =
				scrollbar_max_height * ((box_ymax - box_ymin) / (get_scroll_height()));
			thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

			float scroll_ratio = (scrollbar_max_height - thumb_height) /
								 (get_scroll_height() - (box_ymax - box_ymin));
			float thumb_offset = scroll_y * scroll_ratio;

			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = std::floor(box_ymin + thumb_offset);
			float ymax = std::floor(box_ymin + thumb_offset + thumb_height);

			state.batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
			state.batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			state.batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			state.batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			state.batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
		if(has_horizontal)
		{
			float scrollbar_max_width =
				(box_xmax - box_xmin) - (has_vertical ? scrollbar_thickness - 1 : 0.f);
			float thumb_width = scrollbar_max_width * ((box_xmax - box_xmin) / get_scroll_width());
			thumb_width = std::max(thumb_width, scrollbar_thumb_min_size);

			float scroll_ratio =
				(scrollbar_max_width - thumb_width) / (get_scroll_width() - (box_xmax - box_xmin));
			float thumb_offset = scroll_x * scroll_ratio;

			float xmin = std::floor(box_xmin + thumb_offset);
			float xmax = std::floor(box_xmin + thumb_offset + thumb_width);
			float ymin = box_ymax - scrollbar_thickness;
			float ymax = box_ymax;

			state.batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
			state.batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			state.batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			state.batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			state.batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
	}
}

bool text_prompt_wrapper::internal_draw_pretext()
{
	// this is bad code, but I am surprised it works.
	float lineskip = get_lineskip();

	if(y_scrollable() || x_scrollable())
	{
		// find the cursor to check if we need to move the scroll.
		// this is mainly for the worst case which is if the cursor is below the box.
		// this could be slow?, could be optimized by caching the previous scroll area.
		// probably not neccessary because STB's whole API slow (but simple!) so just refactor
		// everything.

		if(x_scrollable())
		{
			scroll_w = 0;
		}

		size_t cur = 0;
		size_t end = text_data.size();
		float cur_x = 0;
		float cur_y = 0;
		float found_cur_x = -1;
		float found_cur_y = -1;
#ifndef NDEBUG
		bool found_cur = false;
#endif
		while(cur != end)
		{
			StbTexteditRow r;
			STB_TEXTEDIT_LAYOUTROW(&r, this, cur);

			// if we can scroll horizontally, we need to find the width of every line.
			if(x_scrollable())
			{
				int i;
				float width = 0;
				for(i = 0; i != r.num_chars; ++i)
				{
					prompt_char c = text_data.at(cur + i);
					if(c.codepoint != '\n')
					{
						width += c.advance;
					}
				}
				scroll_w = std::max(scroll_w, width);
			}

			if(scroll_to_cursor)
			{
				if(stb_state.cursor >= static_cast<int>(cur) &&
				   stb_state.cursor <= static_cast<int>(cur + r.num_chars))
				{
					if(x_scrollable() || y_scrollable())
					{
						cur_x = 0;
						int i;
						for(i = 0; i != r.num_chars; ++i)
						{
							if(static_cast<int>(cur + i) == stb_state.cursor)
							{
#ifndef NDEBUG
								// ASSERT(!found_cur);
								found_cur = true;
#endif
								found_cur_x = cur_x;
								found_cur_y = cur_y;
								break;
							}
							if(text_data.at(cur + i).codepoint != '\n')
							{
								cur_x += text_data.at(cur + i).advance;
							}
						}
						if(i == r.num_chars && ((static_cast<int>(cur + i) == stb_state.cursor)))
						{
#ifndef NDEBUG
							// ASSERT(!found_cur);
							found_cur = true;
#endif
							if(!text_data.empty() && text_data.at(cur + i - 1).codepoint == '\n')
							{
								found_cur_x = 0;
								found_cur_y = cur_y + lineskip;
							}
							else
							{
								found_cur_x = cur_x;
								found_cur_y = cur_y;
							}
						}
					}
				}

				// this is a hint if the cursor is below the view,
				// this could still be as bad as starting from the beginning,
				// but most of the time it's probably just one line away.
				if(cur_y < scroll_y)
				{
					// first_pass_hint = static_cast<int>(cur);
				}
			}

			cur += r.num_chars;
			if(cur != end)
			{
				// don't do this at the end because the newline decides if there is a lineskip.
				cur_y += lineskip;
			}
		}
		if(!text_data.empty() && text_data.back().codepoint == '\n')
		{
			cur_y += lineskip;
			cur_x = 0;
		}

		if(stb_state.cursor == static_cast<int>(end))
		{
			if((x_scrollable() || y_scrollable()) && scroll_to_cursor)
			{
#ifndef NDEBUG
				// ASSERT(!found_cur);
				found_cur = true;
#endif
				found_cur_x = cur_x;
				found_cur_y = cur_y;
			}
		}
#ifndef NDEBUG
		if(scroll_to_cursor && (x_scrollable() || y_scrollable()))
		{
			ASSERT(found_cur && "cursor not found");
		}
#endif

		if(y_scrollable())
		{
			scroll_h = cur_y + lineskip;
		}

		// scroll to cursor clamping.
		if(!text_data.empty() && scroll_to_cursor)
		{
			bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
			bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));

			if(y_scrollable())
			{
				if(found_cur_y < scroll_y)
				{
					scroll_y = found_cur_y;
				}
				else if(
					found_cur_y + lineskip + (has_horizontal ? scrollbar_thickness : 0) >=
					scroll_y + (box_ymax - box_ymin))
				{
					scroll_y = found_cur_y + lineskip - (box_ymax - box_ymin) +
							   (has_horizontal ? scrollbar_thickness : 0);
				}
			}
			if(x_scrollable())
			{
				if(found_cur_x < scroll_x)
				{
					scroll_x = found_cur_x;
				}
				else if(
					found_cur_x + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding >=
					scroll_x + (box_xmax - box_xmin))
				{
					scroll_x = found_cur_x - (box_xmax - box_xmin) +
							   (has_vertical ? scrollbar_thickness : 0) + horizontal_padding;
				}
			}
		}

		// clamp the the scroll (this happens when you move the scrollbar)
		if(y_scrollable())
		{
			bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
			scroll_y = std::max(
				0.f,
				std::min(
					scroll_h + (has_horizontal ? scrollbar_thickness : 0) - (box_ymax - box_ymin),
					scroll_y));
		}
		if(x_scrollable())
		{
			bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
			scroll_x = std::max(
				0.f,
				std::min(
					scroll_w + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding -
						(box_xmax - box_xmin),
					scroll_x));
		}
		scroll_to_cursor = false;
	}
	return true;
}

bool text_prompt_wrapper::internal_draw_text(
	size_t offset, bool* caret_visible, float* caret_x, float* caret_y)
{
	auto white_uv = state.font->get_font_atlas()->white_uv;

	// set the batcher position.
	float anchor_x_pos = box_xmin - (x_scrollable() ? scroll_x : 0.f);
	state.set_xy(anchor_x_pos, box_ymin - (y_scrollable() ? scroll_y : 0.f));

	// std::array<uint8_t, 4> active_text_color = text_color;
	// painter->set_color(text_color);

	float lineskip = get_lineskip();

	size_t selection_start = std::min(stb_state.select_start, stb_state.select_end);
	size_t selection_end = std::max(stb_state.select_start, stb_state.select_end);
	bool currently_selected = false;
	int selection_vertex_buffer_index = -1;
	float selection_minx = -1;
	std::array<uint8_t, 4> active_selection_color =
		(text_focus) ? select_fill_color : unfocused_select_fill_color;

	int backdrop_vertex_buffer_index = -1;
	float backdrop_minx = -1;
	int active_color_index = -1;

	size_t i = offset;
	size_t cur = offset;
	size_t end = text_data.size();

	while(cur != end)
	{
		StbTexteditRow r;
		STB_TEXTEDIT_LAYOUTROW(&r, this, i);

		i = i + r.num_chars;
		ASSERT(i <= text_data.size());

		if(y_scrollable() && cull_box() && state.draw_y_pos + lineskip <= box_ymin)
		{
			if(STB_TEXT_HAS_SELECTION(&stb_state))
			{
				// don't forget the selection!
				if(selection_start >= cur && selection_start < i)
				{
					currently_selected = true;
				}
				if(selection_end >= cur && selection_end < i)
				{
					currently_selected = false;
				}
			}
			cur = i;
			// newline
			state.draw_y_pos += lineskip;
			continue;
		}

		if(draw_backdrop())
		{
			backdrop_minx = state.draw_x_pos;
			backdrop_vertex_buffer_index = state.batcher->placeholder_rect();
			if(backdrop_vertex_buffer_index == -1)
			{
				// return false;
				return true;
			}
			// this is a signal that you should set active_color_index to the current char.
			active_color_index = -1;
		}

		if(currently_selected && selection_vertex_buffer_index == -1)
		{
			selection_minx = state.draw_x_pos;
			// NOTE: this will create a rect even if it's empty...
			selection_vertex_buffer_index = state.batcher->placeholder_rect();
			if(selection_vertex_buffer_index == -1)
			{
				// return false;
				return true;
			}
			// active_text_color = select_text_color;
		}

		for(; cur != i; ++cur)
		{
			prompt_char ret = text_data.at(cur);
			if(draw_backdrop())
			{
				if(active_color_index != -1 && active_color_index != ret.color_index)
				{
					std::array<uint8_t, 4> color =
						(active_color_index == 0 ? backdrop_color
												 : color_table[active_color_index - 1].back);

					// end the backdrop
					ASSERT(backdrop_vertex_buffer_index != -1);
					float pos_x = cull_box() ? std::max(box_xmin, backdrop_minx) : backdrop_minx;
					float pos_w =
						cull_box() ? std::min(box_xmax, state.draw_x_pos) : state.draw_x_pos;
					float pos_y = state.draw_y_pos;
					float pos_h = state.draw_y_pos + lineskip;
					if(!state.batcher->draw_rect_at(
						   backdrop_vertex_buffer_index,
						   {pos_x, pos_y, pos_w, pos_h},
						   white_uv,
						   color))
					{
						// return false;
						return true;
					}
					// backdrop_vertex_buffer_index = -1;
					// start a new backdrop
					backdrop_minx = state.draw_x_pos;
					backdrop_vertex_buffer_index = state.batcher->placeholder_rect();
					if(backdrop_vertex_buffer_index == -1)
					{
						// return false;
						return true;
					}
				}
				active_color_index = ret.color_index;
			}

			if(STB_TEXT_HAS_SELECTION(&stb_state))
			{
				if(cur == selection_start)
				{
					ASSERT(selection_vertex_buffer_index == -1);
					// start selection
					currently_selected = true;
					selection_minx = state.draw_x_pos;
					selection_vertex_buffer_index = state.batcher->placeholder_rect();
					if(selection_vertex_buffer_index == -1)
					{
						// return false;
						return true;
					}
					// active_text_color = select_text_color;
				}
				else if(cur == selection_end)
				{
					// end selection
					currently_selected = false;
					float pos_x = cull_box() ? std::max(box_xmin, selection_minx) : selection_minx;
					float pos_w =
						cull_box() ? std::min(box_xmax, state.draw_x_pos) : state.draw_x_pos;
					float pos_y = state.draw_y_pos;
					float pos_h = state.draw_y_pos + lineskip;
					if(!state.batcher->draw_rect_at(
						   selection_vertex_buffer_index,
						   {pos_x, pos_y, pos_w, pos_h},
						   white_uv,
						   active_selection_color))
					{
						// return false;
						return true;
					}
					selection_vertex_buffer_index = -1;
					// active_text_color = text_color;
				}
			}

			if(static_cast<int>(cur) == stb_state.cursor)
			{
				if(caret_x != NULL) *caret_x = state.draw_x_pos;
				if(caret_y != NULL) *caret_y = state.draw_y_pos;
				if(caret_visible != NULL) *caret_visible = true;
			}

			if(ret.codepoint == '\t')
			{
				// state.insert_padding(space_advance_cache * 4);
				state.draw_x_pos += space_advance_cache * 4;
			}
			else if(!single_line() && ret.codepoint == '\n')
			{
				// make the newline draw a selected area to show you are selecting the newline
				// NOTE: the backdrop looks ugly with this on, so I removed it if you use it.
				if(!draw_backdrop())
				{
					state.draw_x_pos += space_advance_cache;
				}
			}
			else
			{
				if(cull_box() &&
				   (state.draw_x_pos + ret.advance < box_xmin || state.draw_x_pos >= box_xmax))
				{
					// state.insert_padding(ret.advance);
					state.draw_x_pos += ret.advance;
				}
				else
				{
					if(ret.color_index != 0 && ret.color_index > color_table_size)
					{
						serrf(
							"%s color index out of bounds: %uc, size: %zu\n",
							__func__,
							ret.color_index - 1,
							color_table_size);
						return false;
					}
					std::array<uint8_t, 4> color =
						(currently_selected
							 ? select_text_color
							 : (ret.color_index == 0 ? text_color
													 : color_table[ret.color_index - 1].fore));
#if 0
                    // this is fine, but personally I love extra thick oulines (with force_bitmap),
                    // and if I draw the outline between each letter instead of between the whole text,
                    // the outline would make the text harder to read...
                    float peek_x = state.draw_x_pos;
					switch(state.load_glyph_verts(ret.codepoint, {0,0,0,255}, FONT_STYLE_OUTLINE))
					{
					case FONT_RESULT::NOT_FOUND:
						serrf("%s glyph not found: U+%X\n", __func__, ret.codepoint);
						return false;
					case FONT_RESULT::ERROR: return false;
					case FONT_RESULT::SUCCESS: break;
					}
                    state.draw_x_pos = peek_x;
#endif
					switch(state.load_glyph_verts(ret.codepoint, color, ret.style))
					{
					case FONT_BASIC_RESULT::NOT_FOUND:
						serrf("%s glyph not found: U+%X\n", __func__, ret.codepoint);
						return false;
					case FONT_BASIC_RESULT::ERROR: return false;
					case FONT_BASIC_RESULT::SUCCESS: break;
					}
				}
			}
		}

		if(draw_backdrop())
		{
			std::array<uint8_t, 4> color =
				(active_color_index == 0 ? backdrop_color
										 : color_table[active_color_index - 1].back);
			ASSERT(backdrop_vertex_buffer_index != -1);
			float pos_x = cull_box() ? std::max(box_xmin, backdrop_minx) : backdrop_minx;
			float pos_w = cull_box() ? std::min(box_xmax, state.draw_x_pos) : state.draw_x_pos;
			float pos_y = state.draw_y_pos;
			float pos_h = state.draw_y_pos + lineskip;
			if(!state.batcher->draw_rect_at(
				   backdrop_vertex_buffer_index, {pos_x, pos_y, pos_w, pos_h}, white_uv, color))
			{
				// return false;
				return true;
			}
			backdrop_vertex_buffer_index = -1;
		}

		if(currently_selected)
		{
			// end of row, finish the selection
			ASSERT(selection_vertex_buffer_index != -1);
			float pos_x = cull_box() ? std::max(box_xmin, selection_minx) : selection_minx;
			float pos_w = cull_box() ? std::min(box_xmax, state.draw_x_pos) : state.draw_x_pos;
			float pos_y = state.draw_y_pos;
			float pos_h = state.draw_y_pos + lineskip;
			if(!state.batcher->draw_rect_at(
				   selection_vertex_buffer_index,
				   {pos_x, pos_y, pos_w, pos_h},
				   white_uv,
				   active_selection_color))
			{
				// return false;
				return true;
			}
			selection_vertex_buffer_index = -1;
		}

		if(cur != end)
		{
			// newline
			state.draw_x_pos = anchor_x_pos;
			state.draw_y_pos += lineskip;

			if(cull_box() && state.draw_y_pos >= box_ymax)
			{
				break;
			}

			/*if(currently_selected)
			{
				// prepare the next row's selection
				selection_minx = batcher->draw_x_pos();
				batcher->set_color(active_selection_color);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				selection_vertex_buffer_index = batcher->font_vertex_buffer.size();
				batcher->draw_rect(0, 0, 0, 0);
				batcher->set_color(select_text_color);
			}
			if(draw_backdrop())
			{
				// prepare the next row's backdrop
				backdrop_minx = batcher->draw_x_pos();
				batcher->set_color(backdrop_color);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				backdrop_vertex_buffer_index = batcher->font_vertex_buffer.size();
				batcher->draw_rect(0, 0, 0, 0);
				batcher->set_color(text_color);
			}*/
		}
	}

	if(cur == end && static_cast<int>(end) == stb_state.cursor)
	{
		// This needs to be done because stb's layout function will not
		// be called again if there is a newline at the end,
		// because a empty layout would return num_chars = 0, leading to inf loop
		if(!text_data.empty() && text_data.back().codepoint == '\n')
		{
			// newline
			state.draw_x_pos = anchor_x_pos;
			state.draw_y_pos += lineskip;
		}

		if(cull_box() && state.draw_y_pos > box_ymax)
		{
		}
		else
		{
			// cursor at the end of the line
			if(caret_x != NULL) *caret_x = state.draw_x_pos;
			if(caret_y != NULL) *caret_y = state.draw_y_pos;
			if(caret_visible != NULL) *caret_visible = true;
		}
	}
	return true;
}

bool text_prompt_wrapper::internal_draw_marked(float x, float y)
{
	ASSERT(text_focus);
	ASSERT(!read_only());

	auto white_uv = state.font->get_font_atlas()->white_uv;

	float lineskip = get_lineskip();

	// reserve space to draw a backdrop,
	int marked_vertex_buffer_index = state.batcher->placeholder_rect();
	if(marked_vertex_buffer_index == -1)
	{
		// return false;
		return true;
	}
	state.set_xy(x, y);
	// draw the text
	auto str_cur = markedText.begin();
	auto str_end = markedText.end();
	float marked_caret_x = -1;
	float marked_caret_y = -1;

	// TODO(dootsie): I haven't implemented marked_cursor_end IME selection
	// because I can't test it since I don't have wayland
	marked_caret_x = state.draw_x_pos;
	marked_caret_y = state.draw_y_pos;

	size_t marked_text_buffer_index = state.batcher->get_current_vertex_count();

	while(str_cur != str_end)
	{
#ifdef IME_TEXTEDIT_EXT
		if(draw_caret && std::distance(markedText.begin(), str_cur) == marked_cursor_begin)
		{
			marked_caret_x = state.draw_x_pos;
			marked_caret_y = state.draw_y_pos;
		}
#endif

		uint32_t codepoint;
		utf8::internal::utf_error err_code =
			utf8::internal::validate_next(str_cur, str_end, codepoint);
		if(err_code != utf8::internal::UTF8_OK)
		{
			slogf("info: %s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			break;
		}

		switch(state.load_glyph_verts(codepoint, text_color, FONT_STYLE_NORMAL))
		{
		case FONT_BASIC_RESULT::NOT_FOUND:
			serrf("%s glyph not found: U+%X\n", __func__, codepoint);
			return false;
		case FONT_BASIC_RESULT::ERROR: return false;
		case FONT_BASIC_RESULT::SUCCESS: break;
		}
	}
#ifdef IME_TEXTEDIT_EXT
	if(std::distance(markedText.begin(), str_cur) == marked_cursor_begin)
	{
		// get the caret at the end
		marked_caret_x = state.draw_x_pos;
		marked_caret_y = state.draw_y_pos;
	}
#endif

	float pos_x = x;
	float pos_w = state.draw_x_pos;
	float pos_y = y;
	float pos_h = y + lineskip;

	// check if the area is within bounds
	// Keep the camera in bounds
	// this is a pretty nasty hack (I implemented get advance now which means I should replace
	// this!)
	if(cull_box() && pos_w > box_xmax)
	{
		float x_off = pos_w - box_xmax;
		size_t size = state.batcher->get_current_vertex_count();
		for(; marked_text_buffer_index < size; ++marked_text_buffer_index)
		{
			state.batcher->buffer[marked_text_buffer_index].pos[0] -= x_off;
		}
		pos_x -= x_off;
		pos_w -= x_off;
		marked_caret_x -= x_off;
	}

	// finish the backdrop
	if(!state.batcher->draw_rect_at(
		   marked_vertex_buffer_index, {pos_x, pos_y, pos_w, pos_h}, white_uv, backdrop_color))
	{
		// return false;
		return true;
	}

	// draw the cursor
	if(draw_caret)
	{
		state.batcher->draw_rect(
			{marked_caret_x, marked_caret_y, marked_caret_x + 2, marked_caret_y + lineskip},
			white_uv,
			caret_color);
	}

	// the location for the IME text
	SDL_Rect r;
	r.x = static_cast<int>(pos_x);
	r.w = static_cast<int>(pos_w - pos_x);
	r.y = static_cast<int>(pos_y);
	r.h = static_cast<int>(pos_h - pos_y);
	SDL_SetTextInputRect(&r);
	return true;
}

bool text_prompt_wrapper::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	ASSERT(y_scrollable());
	bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
	bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
	if(has_vertical)
	{
		float scrollbar_max_height =
			(box_ymax - box_ymin) - (has_horizontal ? scrollbar_thickness - 1 : 0.f);

		float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / get_scroll_height());
		thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

		float scroll_ratio =
			(scrollbar_max_height - thumb_height) / (get_scroll_height() - (box_ymax - box_ymin));
		float thumb_offset = scroll_y * scroll_ratio;

		float xmin = box_xmax - scrollbar_thickness;
		float xmax = box_xmax;
		float ymin = box_ymin + thumb_offset;
		float ymax = box_ymin + thumb_offset + thumb_height;

		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			scroll_thumb_click_offset = mouse_y - thumb_offset;
			return true;
		}
	}
	return false;
}

bool text_prompt_wrapper::internal_scroll_x_inside(float mouse_x, float mouse_y)
{
	ASSERT(x_scrollable());
	bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
	bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
	if(has_horizontal)
	{
		float scrollbar_max_width =
			(box_xmax - box_xmin) - (has_vertical ? scrollbar_thickness - 1 : 0.f);
		float thumb_width = scrollbar_max_width * ((box_xmax - box_xmin) / get_scroll_width());
		thumb_width = std::max(thumb_width, scrollbar_thumb_min_size);

		float scroll_ratio =
			(scrollbar_max_width - thumb_width) / (get_scroll_width() - (box_xmax - box_xmin));
		float thumb_offset = (scroll_x * scroll_ratio);

		float xmin = box_xmin + thumb_offset;
		float xmax = box_xmin + thumb_offset + thumb_width;
		float ymin = box_ymax - scrollbar_thickness;
		float ymax = box_ymax;
		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			scroll_thumb_click_offset = mouse_x - thumb_offset;
			return true;
		}
	}
	return false;
}

void text_prompt_wrapper::internal_scroll_y_to(float mouse_y)
{
	ASSERT(y_scrollable());
	bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
	bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
	if(has_vertical)
	{
		float scrollbar_max_height =
			(box_ymax - box_ymin) - (has_horizontal ? scrollbar_thickness - 1 : 0.f);

		float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / get_scroll_height());
		thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

		float scroll_ratio =
			(scrollbar_max_height - thumb_height) / (get_scroll_height() - (box_ymax - box_ymin));
		scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;
	}
}

void text_prompt_wrapper::internal_scroll_x_to(float mouse_x)
{
	ASSERT(x_scrollable());
	bool has_vertical = (y_scrollable() && get_scroll_height() > (box_ymax - box_ymin));
	bool has_horizontal = (x_scrollable() && get_scroll_width() > (box_xmax - box_xmin));
	if(has_horizontal)
	{
		float scrollbar_max_width =
			(box_xmax - box_xmin) - (has_vertical ? scrollbar_thickness - 1 : 0.f);

		float thumb_width = scrollbar_max_width * ((box_xmax - box_xmin) / get_scroll_width());
		thumb_width = std::max(thumb_width, scrollbar_thumb_min_size);

		float scroll_ratio =
			(scrollbar_max_width - thumb_width) / (get_scroll_width() - (box_xmax - box_xmin));
		scroll_x = (mouse_x - scroll_thumb_click_offset) / scroll_ratio;
	}
}

TEXT_PROMPT_RESULT text_prompt_wrapper::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			if(text_focus)
			{
				unfocus();
				return TEXT_PROMPT_RESULT::UNFOCUS;
			}
			return TEXT_PROMPT_RESULT::CONTINUE;
			// leave is only used for releasing "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
		break;

	// lazy scroll
	case SDL_MOUSEWHEEL:
		if(y_scrollable())
		{
			// only scroll when the mouse is currently hovering over the bounding box
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			float mouse_x = static_cast<float>(x);
			float mouse_y = static_cast<float>(y);
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				// This gets corrected in pre_text,
				// NOTE: but there is a minor bug where you drag text at the top of the scroll,
				// and scroll up, the selection will move up, same for the bottom.
				// This could be fixed by finding the cursor during drawing instead.
				scroll_y -= static_cast<float>(e.wheel.y * cv_scroll_speed.data) * get_lineskip();
				// the draw function will clamp it to keep the scroll area inside of the text.
				update_buffer = true;
				// NOTE: this is weird, but I think I can use leave to prevent multiple things
				// scrolling.
				set_event_leave(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
		}
		break;
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		if(mouse_held)
		{
			ASSERT(text_focus);
			drag_x = mouse_x;
			drag_y = mouse_y;
			scroll_to_cursor = true;
			blink_timer = timer_now();
			update_buffer = true;
		}
		if(y_scrollbar_held)
		{
			ASSERT(text_focus);
			internal_scroll_y_to(mouse_y);
			update_buffer = true;
		}
		if(x_scrollbar_held)
		{
			ASSERT(text_focus);
			internal_scroll_x_to(mouse_x);
			update_buffer = true;
		}
		// helps unfocus other elements.
		// TODO(dootsie): probably should exclude the gap between the scrollbar....
		if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x && box_xmin <= mouse_x)
		{
			// eat
			set_event_leave(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			if(mouse_held)
			{
				ASSERT(text_focus);
				mouse_held = false;
				drag_x = -1;
				drag_y = -1;
				scroll_to_cursor = true;
				stb_textedit_drag(this, &stb_state, mouse_x, mouse_y);
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}

			if(y_scrollable() && y_scrollbar_held)
			{
				ASSERT(text_focus);
				y_scrollbar_held = false;
				internal_scroll_y_to(mouse_y);
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			if(x_scrollable() && x_scrollbar_held)
			{
				ASSERT(text_focus);
				x_scrollbar_held = false;
				internal_scroll_x_to(mouse_x);
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}

			// don't let this event leak if in focus because the only way to unfocus
			// should be from a button down.
			if(text_focus)
			{
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}

			// helps unfocus other elements.
			// TODO(dootsie): probably should exclude the gap between the scrollbar....
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			if(y_scrollable() && internal_scroll_y_inside(mouse_x, mouse_y))
			{
				y_scrollbar_held = true;
				internal_scroll_y_to(mouse_y);
				focus();
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}

			if(!single_line() && x_scrollable() && internal_scroll_x_inside(mouse_x, mouse_y))
			{
				x_scrollbar_held = true;
				internal_scroll_x_to(mouse_x);
				focus();
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}

			// NOTE: I could make it so that if you click in the empty part of the scrollbar, it
			// moves it, but I don't know if I want that.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				focus();

				scroll_to_cursor = true;
				// this is neccessary!
				update_buffer = true;

				// when you click while markedText is not empty,
				// I just move the whole IME text to that location,
				// which is annoying but I WANT the leftover IME text
				// to be pasted, but there is no way to do that...
				// I could use markedText, but it is terrible
				// because long strings fragment...
				if(markedText.empty())
				{
					mouse_held = true;
					drag_x = mouse_x;
					drag_y = mouse_y;
				}

				// when you hold shift for a selection
				if((SDL_GetModState() & KMOD_SHIFT) != 0 && markedText.empty())
				{
					stb_textedit_drag(this, &stb_state, mouse_x, mouse_y);
				}
				else
				{
					stb_textedit_click(this, &stb_state, mouse_x, mouse_y);
				}
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
            if(text_focus)
            {
                unfocus();
				return TEXT_PROMPT_RESULT::UNFOCUS;
            }
		}
		break;
		// Special text input event
	case SDL_TEXTINPUT: {
		if(!text_focus)
		{
			break;
		}

		if(read_only())
		{
			break;
		}
		// slogf("Keyboard: text input \"%s\"\n", e.text.text);

		scroll_to_cursor = true;
		mouse_held = false;
		drag_x = -1;
		drag_y = -1;

		size_t text_len = strlen(e.text.text);

		std::u32string wstr;
		wstr.reserve(text_len);
		const char* str_cur = e.text.text;
		const char* str_end = e.text.text + text_len;

		while(str_cur != str_end)
		{
			uint32_t codepoint;
			utf8::internal::utf_error err_code =
				utf8::internal::validate_next(str_cur, str_end, codepoint);
			if(err_code != utf8::internal::UTF8_OK)
			{
				slogf("info: %s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
				break;
			}
			wstr.push_back(codepoint);
		}

		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		stb_textedit_paste(this, &stb_state, wstr.data(), wstr.size());

		// clean IME text if it exists.
		markedText.clear();

		blink_timer = timer_now();
		update_buffer = true;
		// eat
		set_event_unfocus(e);
		return TEXT_PROMPT_RESULT::CONTINUE;
	}

	// this might just be a linux problem, but on version 2.0.0
	// it wont send a event with "" for clearing the text editing
	// when you backspace the IME to go away.
	// not sure which version it gets fixed.
	case SDL_TEXTEDITING:
		if(!text_focus)
		{
			break;
		}
		if(read_only())
		{
			break;
		}

		// slogf("Keyboard: text edit \"%s\", start: %d, length:
		// %d\n",e.edit.text,e.edit.start,e.edit.length);

		if(e.edit.text[0] == '\0')
		{
			// because I would click into a prompt,
			// that would trigger this with an empty string, and it would prevent dragging.
			// NOTE: double check if making a SDL_TEXTEDITING chain would cause the next
			// SDL_TEXTEDITING to be empty?
			markedText.clear();
		}
        else
        {
            scroll_to_cursor = true;
            mouse_held = false;
            drag_x = -1;
            drag_y = -1;

            // IME text
            if(stb_state.select_start != stb_state.select_end)
            {
                // I want the IME text to be at the beginning of the line.
                if(stb_state.select_start < stb_state.select_end)
                {
                    stb_state.cursor = stb_state.select_start;
                    int temp = stb_state.select_start;
                    stb_state.select_start = stb_state.select_end;
                    stb_state.select_end = temp;
                }
                // delete the selection.
                stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_BACKSPACE);
            }
            markedText = e.edit.text;

        }

		blink_timer = timer_now();
		update_buffer = true;

		// eat
		set_event_unfocus(e);
		return TEXT_PROMPT_RESULT::CONTINUE;

#ifdef IME_TEXTEDIT_EXT
		// requires SDL_HINT_IME_SUPPORT_EXTENDED_TEXT
		// this actually properly uses the cursor and selection because
		// the old version had a limited string length.
		// in old SDL the start + length was used for splitting the events into chunks
	case SDL_TEXTEDITING_EXT:
		// DONT MIX UP .edit with .editExt
		if(!text_focus)
		{
			break;
		}
		if(read_only())
		{
			break;
		}
		slogf(
			"Keyboard: text editExt \"%s\", start: %d, length: %d\n",
			e.editExt.text,
			e.editExt.start,
			e.editExt.length);

		if(e.editExt.text[0] == '\0')
		{
			// because I would click into a prompt,
			// that would trigger this with an empty string, and it would prevent dragging.
			markedText.clear();
		}
		else
		{
			scroll_to_cursor = true;
			mouse_held = false;
			drag_x = -1;
			drag_y = -1;

			marked_cursor_begin = e.editExt.start;
			marked_cursor_end = e.editExt.length;
			// IME text
			if(stb_state.select_start != stb_state.select_end)
			{
				// I want the IME text to be at the beginning of the line.
				if(stb_state.select_start < stb_state.select_end)
				{
					stb_state.cursor = stb_state.select_start;
					int temp = stb_state.select_start;
					stb_state.select_start = stb_state.select_end;
					stb_state.select_end = temp;
				}
				// delete the selection.
				stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_BACKSPACE);
			}
			markedText = e.editExt.text;
		}

		blink_timer = timer_now();
		update_buffer = true;

		// eat
		set_event_unfocus(e);
		return TEXT_PROMPT_RESULT::CONTINUE;
#endif
	case SDL_KEYDOWN: {
		// maybe escape unfocus could be a flag if it causes problems
		// or make escape cause the line to be cleared
        // note this is done HERE because I don't want mouse_held = false
		if(text_focus && e.key.keysym.sym == SDLK_ESCAPE)
		{
			unfocus();
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::UNFOCUS;
		}
		if(!text_focus)
		{
			break;
		}

		// NOTE: I haven't actually checked if this makes sense.
		mouse_held = false;

		SDL_Keycode key_shift_mod =
			((e.key.keysym.mod & KMOD_SHIFT) != 0 ? STB_TEXTEDIT_K_SHIFT : 0);
		SDL_Keycode key_ctrl_mod =
			((e.key.keysym.mod & KMOD_CTRL) != 0 ? STB_TEXTEDIT_K_CONTROL : 0);
		switch(e.key.keysym.sym)
		{
		case SDLK_BACKSPACE:
		//case SDLK_DELETE:
		case SDLK_TAB:
		case SDLK_RETURN: {
			if(read_only())
			{
                // should this be eaten?
				break;
			}
			// this isn't in the switch statement because "break" doesn't work.
			if(single_line() && e.key.keysym.sym == SDLK_RETURN)
			{
				break;
			}
			STB_TEXTEDIT_KEYTYPE stb_key;
			switch(e.key.keysym.sym)
			{
			case SDLK_BACKSPACE: stb_key = STB_TEXTEDIT_K_BACKSPACE; break;
            // I disable DELETE because the cursor would move weird when I would undo.
			//case SDLK_DELETE: stb_key = STB_TEXTEDIT_K_DELETE; break;
			case SDLK_TAB: stb_key = '\t'; break;
			case SDLK_RETURN: stb_key = '\n'; break; // note SDLK_RETURN is '\r' for some reason.
			}
			stb_textedit_key(this, &stb_state, stb_key);

			scroll_to_cursor = true;
			blink_timer = timer_now();
			update_buffer = true;
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		}
		// copy
		case SDLK_c:
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				if(STB_TEXT_HAS_SELECTION(&stb_state))
				{
					auto start =
						text_data.begin() + std::min(stb_state.select_start, stb_state.select_end);
					auto end =
						text_data.begin() + std::max(stb_state.select_start, stb_state.select_end);
					std::string out;
					for(; start != end; ++start)
					{
						if(!cpputf_append_string(out, start->codepoint))
						{
							slogf("info: invalid codepoint from prompt: U+%X\n", start->codepoint);
						}
					}
					if(SDL_SetClipboardText(out.c_str()) != 0)
					{
						slogf("info: Failed to set clipboard! SDL Error: %s\n", SDL_GetError());
					}
				}
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		// cut
		case SDLK_x:
			if(read_only())
			{
				break;
			}
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				if(STB_TEXT_HAS_SELECTION(&stb_state))
				{
					scroll_to_cursor = true;
					auto start =
						text_data.begin() + std::min(stb_state.select_start, stb_state.select_end);
					auto end =
						text_data.begin() + std::max(stb_state.select_start, stb_state.select_end);
					std::string out;
					for(; start != end; ++start)
					{
						if(!cpputf_append_string(out, start->codepoint))
						{
							slogf("info: invalid codepoint from prompt: U+%X\n", start->codepoint);
						}
					}
					if(stb_textedit_cut(this, &stb_state) != 1)
					{
						slog("info: didn't cut\n");
					}
					update_buffer = true;

					if(SDL_SetClipboardText(out.c_str()) != 0)
					{
						slogf("info: Failed to set clipboard! SDL Error: %s\n", SDL_GetError());
					}
				}
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		// Handle paste
		case SDLK_v:
			if(read_only())
			{
				break;
			}
			if((e.key.keysym.mod & KMOD_CTRL) != 0 && SDL_HasClipboardText() == SDL_TRUE)
			{
				scroll_to_cursor = true;

				auto sdl_del = [](void* ptr) { SDL_free(ptr); };
				std::unique_ptr<char, decltype(sdl_del)> utext{SDL_GetClipboardText(), sdl_del};

				if(!utext)
				{
					// SDL_HasClipboardText and SDL_GetClipboardText could be a race.
					slogf("info: Failed to get clipboard! SDL Error: %s\n", SDL_GetError());
					break;
				}
				size_t text_len = strlen(utext.get());
				const char* str_cur = utext.get();
				const char* str_end = utext.get() + text_len;

				std::u32string wstr;
				// not accurate but faster than nothing.
				wstr.reserve(text_len);

				while(str_cur != str_end)
				{
					uint32_t codepoint;
					utf8::internal::utf_error err_code =
						utf8::internal::validate_next(str_cur, str_end, codepoint);
					if(err_code != utf8::internal::UTF8_OK)
					{
						slogf("info: %s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
						break;
					}
					wstr.push_back(codepoint);
				}

				// if you paste ontop of a selection, stb has a bug where it will require 2 undo's.
				// but if I used the secret replace function IMGUI uses, I get a ASAN error when I
				// reproduce the same undo/redo bug that causes stb_insert_chars & etc functions
				// which is bad because at least stb_insert_chars & etc can check for it.
				// I think the solution is to use dynamically allocated history,
				// or maybe figure out why IMGUI doesn't have the same problem with weird undo/redo.
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				stb_textedit_paste(this, &stb_state, wstr.data(), wstr.size());
				// if( != 1)
				{
					//	slogf("info: %s failed to paste clipboard: `%s`\n", __func__, utext.get());
				}
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		// arrows
		case SDLK_LEFT:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_LEFT | key_shift_mod | key_ctrl_mod);
			blink_timer = timer_now();
			update_buffer = true;
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		case SDLK_RIGHT:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_RIGHT | key_shift_mod | key_ctrl_mod);
			blink_timer = timer_now();
			update_buffer = true;
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		case SDLK_UP:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			// I need to push back a newline to fix a bug when the cursor is at
			// the end of the text, and it won't move up due to word wrap.
			// how to reproduce: make a long line at the end of the prompt,
			// once it word wraps put the cursor on the second bottom row in an area
			// to the right where there is no text on the wrapped line
			// (the 2nd bottom row must be longer than the wrapped bottom row)
			// then press down then up and you won't move up (without this fix)
			text_data.push_back({'\n', STB_TEXTEDIT_GETWIDTH_NEWLINE, 0, FONT_STYLE_NORMAL});
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_UP | key_shift_mod);
			text_data.pop_back();
			blink_timer = timer_now();
			update_buffer = true;
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		case SDLK_DOWN:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_DOWN | key_shift_mod);
			blink_timer = timer_now();
			update_buffer = true;
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		case SDLK_z:
			if(read_only())
			{
				break;
			}
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				scroll_to_cursor = true;
				stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_UNDO);
				// I don't know why stb doesn't do this for me.
				stb_state.select_end = stb_state.select_start;
				blink_timer = timer_now();
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		case SDLK_y:
			if(read_only())
			{
				break;
			}
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				scroll_to_cursor = true;
				stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_REDO);
				// I don't know why stb doesn't do this for me.
				stb_state.select_end = stb_state.select_start;
				blink_timer = timer_now();
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		case SDLK_a:
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				stb_state.select_start = 0;
				stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
				blink_timer = timer_now();
				update_buffer = true;
				// eat
				set_event_unfocus(e);
				return TEXT_PROMPT_RESULT::CONTINUE;
			}
			break;
		}
	}
	break;
	}
	// SDL will still tigger key events when you SDL_StartTextInput
	// so we got to eat them if we are focused.
	switch(e.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if(!text_focus)
		{
			break;
		}
		if(read_only())
		{
			break;
		}
		// if this is a "printable key" eat the key.
		if((e.key.keysym.sym & SDLK_SCANCODE_MASK) == 0)
		{
			// eat
			set_event_unfocus(e);
			return TEXT_PROMPT_RESULT::CONTINUE;
		}
		break;
	}
	return TEXT_PROMPT_RESULT::CONTINUE;
}

void text_prompt_wrapper::stb_layout_func(StbTexteditRow* row, int i)
{
	auto last_space = text_data.end();
	float last_space_advance = STB_TEXTEDIT_GETWIDTH_NEWLINE;
	float total_advance = box_xmin;

	auto cur = text_data.begin() + i;
	auto start = cur;
	for(; cur != text_data.end(); ++cur)
	{
		if(cur->codepoint == '\n')
		{
			ASSERT(!single_line());
			++cur;
			break;
		}

		// I really want to support U+3000 which is CJK space,
		// but lets pretend like CJK people hate word wrap :)
		if(word_wrap() && cur->codepoint == ' ')
		{
			cur->advance = space_advance_cache;
			total_advance += cur->advance;
			last_space = cur;
			last_space_advance = total_advance;
		}
		else
		{
			if(cur->codepoint == '\t')
			{
				total_advance += space_advance_cache * 4;
			}
			else
			{
				total_advance += cur->advance;
			}
			// I can't remove the padding for the scrollbar because this function is
			// used to calculate scroll_h, which means scroll_h will be incorrect.
			if(!single_line() && !x_scrollable() && cull_box() &&
			   total_advance > box_xmax - scrollbar_thickness)
			{
				if(word_wrap() && last_space != text_data.end())
				{
					last_space->advance = STB_TEXTEDIT_GETWIDTH_NEWLINE;
					cur = last_space + 1;
					total_advance = last_space_advance;
				}
				break;
			}
		}
	}
	// num_chars must be more than 0 or else infinate loop.
	// this happens if the width of the box is 0 or very small
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	row->num_chars = std::max<int>(1, std::distance(start, cur));
	row->x0 = box_xmin - scroll_x;
	row->x1 = total_advance - scroll_x;
	row->ymin = box_ymin - scroll_y;
	row->ymax = box_ymin + get_lineskip() - scroll_y;
	row->baseline_y_delta = get_lineskip();
}

int text_prompt_wrapper::stb_insert_chars(int index, const STB_TEXTEDIT_CHARTYPE* text, int n)
{
	ASSERT(!read_only());
	ASSERT(state.font != NULL);

	if(static_cast<size_t>(index) > text_data.size())
	{
		// yes this does happen because stb undo/redo is broken
		return 0;
	}

	auto it = text_data.insert(text_data.begin() + index, n, prompt_char{});
	for(int i = 0; i < n; ++i, ++it)
	{
		it->color_index = current_color_index;
		it->style = current_style;
		if(single_line() && text[i] == STB_TEXTEDIT_NEWLINE)
		{
			it->codepoint = '#';
		}
		else
		{
			it->codepoint = text[i];
		}
		// this requires the atlas texture to be bound and GL_UNPACK_ALIGNMENT
		if(it->codepoint == '\t')
		{
			it->advance = space_advance_cache * 4;
		}
		else if(!single_line() && it->codepoint == STB_TEXTEDIT_NEWLINE)
		{
			it->advance = STB_TEXTEDIT_GETWIDTH_NEWLINE;
		}
		else
		{
			switch(state.font->get_advance(it->codepoint, &it->advance, state.font_scale))
			{
			case FONT_BASIC_RESULT::SUCCESS: break;
			case FONT_BASIC_RESULT::NOT_FOUND:
				serrf("%s: codepoint not found: U+%X\n", state.font->get_name(), it->codepoint);
				return 0;
			case FONT_BASIC_RESULT::ERROR:
				// TODO: handle this error???
				return 0;
			}
		}
	}
	return 1;
}

void text_prompt_wrapper::stb_delete_chars(int index, int n)
{
	ASSERT(!read_only());
	if(static_cast<size_t>(index) + static_cast<size_t>(n) > text_data.size())
	{
		// yes this does happen because stb undo/redo is broken
		return;
	}
	text_data.erase(text_data.begin() + index, text_data.begin() + (index + n));
}

float text_prompt_wrapper::stb_get_width(int linestart, int index)
{
	if(static_cast<size_t>(linestart) + static_cast<size_t>(index) >= text_data.size())
	{
		// this is actually caused by a hack where I made layoutrow ALWAYS return 1 even if it was
		// zero. happens when I move the cursor to the end of the prompt. I don't know if there are
		// more bugs that are caused by this...
		return STB_TEXTEDIT_GETWIDTH_NEWLINE;
	}
	return text_data.at(linestart + index).advance;
}

STB_TEXTEDIT_CHARTYPE text_prompt_wrapper::stb_get_char(int index)
{
	if(static_cast<size_t>(index) >= text_data.size())
	{
		// yes this does happen because stb undo/redo is broken
		return 0;
	}
	return text_data.at(index).codepoint;
}