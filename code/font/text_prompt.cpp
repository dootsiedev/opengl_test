#include "../global.h"
#include "../cvar.h"
#include "text_prompt.h"
#include "utf8_stuff.h"

// TODO(dootsie): add in double click selection?

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
#define STB_TEXTEDIT_K_DELETE SDLK_DELETE
#define STB_TEXTEDIT_K_INSERT SDLK_INSERT
#define STB_TEXTEDIT_K_WORDLEFT (STB_TEXTEDIT_K_LEFT | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_WORDRIGHT (STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_CONTROL)
#define STB_TEXTEDIT_K_PGUP SDLK_PAGEUP
#define STB_TEXTEDIT_K_PGDOWN SDLK_PAGEDOWN

#define STB_TEXTEDIT_IMPLEMENTATION
#include "../3rdparty/stb_textedit.h"

static REGISTER_CVAR_DOUBLE(
	cv_prompt_scroll_rate, 3, "scroll rate factor of size of a row", CVAR_DEFAULT);

bool text_prompt_wrapper::init(
	std::string_view contents, font_sprite_batcher* batcher_, TEXTP_FLAG flags_)
{
	ASSERT(batcher_ != NULL);
	ASSERT(batcher_->current_anchor == TEXT_ANCHOR::TOP_LEFT);
	batcher = batcher_;
	flags = flags_;

    // does this need to be zero'd?
    stb_textedit_initialize_state(&stb_state, single_line() ? 1 : 0);

	if(!replace_string(contents))
	{
		return false;
	}

	space_advance_cache = batcher->GetAdvance(' ');
	ASSERT(!isnan(space_advance_cache));
	return true;
}

bool text_prompt_wrapper::replace_string(std::string_view contents)
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
			serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			return false;
		}
		wstr.push_back(codepoint);
	}

	// I clear the state because stb_textedit_paste will create 2 undo's
    // otherwise I would only reset the state if this was readonly() to save memory (if it was dynamic).
	stb_textedit_clear_state(&stb_state, single_line() ? 1 : 0);
    text_data.clear();


    //stb_state.select_start = STB_TEXTEDIT_STRINGLEN(this);
	//stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
	//stb_state.cursor = STB_TEXTEDIT_STRINGLEN(this);


	// avoid read only error.
	TEXTP_FLAG was_readonly = flags;
	flags = flags & ~TEXTP_READ_ONLY;
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	// int ret =
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	stb_textedit_paste(this, &stb_state, wstr.data(), wstr.size());
	flags = was_readonly;
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

	return true;
}
void text_prompt_wrapper::clear_string()
{
    update_buffer = true;

    // TODO(dootsie): I could implement this to be undo'able
    // I would make this function use a bool to set wether or not you want keep undo state,
    // and I would need to manually create a undo record, because paste would create 2.
    stb_textedit_clear_state(&stb_state, single_line() ? 1 : 0);
    text_data.clear();

    //stb_state.select_start = STB_TEXTEDIT_STRINGLEN(this);
	//stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
	//stb_state.cursor = STB_TEXTEDIT_STRINGLEN(this);
}

std::string text_prompt_wrapper::get_string() const
{
	std::string out;
	for(const prompt_char& c : text_data)
	{
		if(!cpputf_append_string(out, c.codepoint))
		{
			ASSERT(false && "invalid codepoint from prompt???");
		}
	}
	return out;
}

bool text_prompt_wrapper::draw_requested()
{
	// no great place to put this, so I will put it here.
	ASSERT(!(word_wrap() && single_line()) && "single line can't be wrapped");
	ASSERT(!(y_scrollable() && single_line()) && "single line can't be scrollable");
	ASSERT(!(word_wrap() && x_scrollable()) && "can't have both wordwrap and x scrolling");

	// if you tab out of the window text input can deactivate without sending ""
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
	return update_buffer;
}

bool text_prompt_wrapper::draw()
{
	ASSERT(batcher != NULL);
	ASSERT(batcher->current_anchor == TEXT_ANCHOR::TOP_LEFT);

	float lineskip = batcher->GetLineSkip();

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

	// draw the bbox
	if(draw_bbox())
	{
		batcher->set_color(bbox_color);
		batcher->draw_rect(box_xmin, box_xmin + 1, box_ymin, box_ymax);
		batcher->draw_rect(box_xmin, box_xmax, box_ymin, box_ymin + 1);
		batcher->draw_rect(box_xmax - 1, box_xmax, box_ymin, box_ymax);
		batcher->draw_rect(box_xmin, box_xmax, box_ymax - 1, box_ymax);

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

				batcher->set_color(caret_color);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmin + 1, scrollbar_ymin, scrollbar_ymax);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmax, scrollbar_ymin, scrollbar_ymin + 1);
				batcher->draw_rect(
					scrollbar_xmax - 1, scrollbar_xmax, scrollbar_ymin, scrollbar_ymax);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmax, scrollbar_ymax - 1, scrollbar_ymax);
			}
			if(has_horizontal)
			{
				float scrollbar_max_width =
					(box_xmax - box_xmin) - (has_vertical ? scrollbar_thickness - 1 : 0.f);
				float scrollbar_xmin = box_xmin;
				float scrollbar_xmax = box_xmin + scrollbar_max_width;
				float scrollbar_ymin = box_ymax - scrollbar_thickness;
				float scrollbar_ymax = box_ymax;

				batcher->set_color(caret_color);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmin + 1, scrollbar_ymin, scrollbar_ymax);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmax, scrollbar_ymin, scrollbar_ymin + 1);
				batcher->draw_rect(
					scrollbar_xmax - 1, scrollbar_xmax, scrollbar_ymin, scrollbar_ymax);
				batcher->draw_rect(
					scrollbar_xmin, scrollbar_xmax, scrollbar_ymax - 1, scrollbar_ymax);
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

			batcher->set_color(scrollbar_color);
			batcher->draw_rect(xmin, xmax, ymin, ymax);
			batcher->set_color(bbox_color);
			batcher->draw_rect(xmin, xmin + 1, ymin, ymax);
			batcher->draw_rect(xmin, xmax, ymin, ymin + 1);
			batcher->draw_rect(xmax - 1, xmax, ymin, ymax);
			batcher->draw_rect(xmin, xmax, ymax - 1, ymax);
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

			batcher->set_color(scrollbar_color);
			batcher->draw_rect(xmin, xmax, ymin, ymax);
			batcher->set_color(bbox_color);
			batcher->draw_rect(xmin, xmin + 1, ymin, ymax);
			batcher->draw_rect(xmin, xmax, ymin, ymin + 1);
			batcher->draw_rect(xmax - 1, xmax, ymin, ymax);
			batcher->draw_rect(xmin, xmax, ymax - 1, ymax);
		}
	}

	float caret_x = -1;
	float caret_y = -1;
	bool caret_visible = false;
	if(!internal_draw_text(0, &caret_visible, &caret_x, &caret_y))
	{
		return false;
	}

	if(markedText.empty())
	{
		// draw the cursor
		if(draw_caret && caret_visible)
		{
			if(box_ymax >= caret_y && box_ymin <= caret_y + lineskip && box_xmax >= caret_x &&
			   box_xmin <= caret_x + 2)
			{
				ASSERT(text_focus);
				ASSERT(!read_only());
				batcher->set_color(caret_color);
				batcher->draw_rect(caret_x, caret_x + 2, caret_y, caret_y + lineskip);
			}
		}
	}
	else if(!internal_draw_marked(caret_x, caret_y))
	{
		return false;
	}

	return true;
}

bool text_prompt_wrapper::internal_draw_pretext()
{
	// this is bad code, but I am surprised it works.
	float lineskip = batcher->GetLineSkip();

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
					scroll_w + (has_vertical ? scrollbar_thickness : 0) + horizontal_padding - (box_xmax - box_xmin),
					scroll_x));
		}
		scroll_to_cursor = false;
	}
	return true;
}

bool text_prompt_wrapper::internal_draw_text(
	size_t offset, bool* caret_visible, float* caret_x, float* caret_y)
{
	// set the batcher position.
	batcher->SetXY(
		box_xmin - (x_scrollable() ? scroll_x : 0.f), box_ymin - (y_scrollable() ? scroll_y : 0.f));

	batcher->set_color(text_color);

	float lineskip = batcher->GetLineSkip();

	size_t selection_start = std::min(stb_state.select_start, stb_state.select_end);
	size_t selection_end = std::max(stb_state.select_start, stb_state.select_end);
	bool currently_selected = false;
	int selection_vertex_buffer_index = -1;
	float selection_minx = -1;

	std::array<uint8_t, 4> active_selection_color =
		(text_focus) ? select_fill_color : unfocused_select_fill_color;

	size_t i = offset;
	size_t cur = offset;
	size_t end = text_data.size();

	while(cur != end)
	{
		StbTexteditRow r;
		STB_TEXTEDIT_LAYOUTROW(&r, this, i);

		i = i + r.num_chars;
		ASSERT(i <= text_data.size());

		if(y_scrollable() && cull_box() && batcher->draw_y_pos() + lineskip <= box_ymin)
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
			batcher->Newline();
			continue;
		}

		for(; cur != i; ++cur)
		{
			if(STB_TEXT_HAS_SELECTION(&stb_state))
			{
				if(currently_selected && selection_vertex_buffer_index == -1)
				{
					selection_minx = batcher->draw_x_pos();
					batcher->set_color(active_selection_color);
					// NOLINTNEXTLINE(bugprone-narrowing-conversions)
					selection_vertex_buffer_index = batcher->font_vertex_buffer.size();
					batcher->draw_rect(0, 0, 0, 0);
					batcher->set_color(select_text_color);
				}
				if(cur == selection_start)
				{
					// start selection
					currently_selected = true;
					selection_minx = batcher->draw_x_pos();
					batcher->set_color(active_selection_color);
					// NOLINTNEXTLINE(bugprone-narrowing-conversions)
					selection_vertex_buffer_index = batcher->font_vertex_buffer.size();
					batcher->draw_rect(0, 0, 0, 0);
					batcher->set_color(select_text_color);
				}
				else if(cur == selection_end)
				{
					// end selection
					currently_selected = false;
					float pos_x = cull_box() ? std::max(box_xmin, selection_minx) : selection_minx;
					float pos_w = cull_box() ? std::min(box_xmax, batcher->draw_x_pos())
											 : batcher->draw_x_pos();
					float pos_y = batcher->draw_y_pos();
					float pos_h = batcher->draw_y_pos() + lineskip;
					batcher->move_rect(selection_vertex_buffer_index, pos_x, pos_w, pos_y, pos_h);
					selection_vertex_buffer_index = 0;

					batcher->set_color(text_color);
				}
			}

			prompt_char ret = text_data.at(cur);

			if(static_cast<int>(cur) == stb_state.cursor)
			{
				if(caret_x != NULL) *caret_x = batcher->draw_x_pos();
				if(caret_y != NULL) *caret_y = batcher->draw_y_pos();
				if(caret_visible != NULL) *caret_visible = true;
			}

			if(ret.codepoint == '\t')
			{
				batcher->insert_padding(space_advance_cache * 4);
			}
			else if(!single_line() && ret.codepoint == '\n')
			{
				// make the newline draw a selected area to show you are selecting the newline
				batcher->insert_padding(space_advance_cache);
			}
			else
			{
				if(cull_box() && (batcher->draw_x_pos() + ret.advance < box_xmin ||
								  batcher->draw_x_pos() >= box_xmax))
				{
					batcher->insert_padding(ret.advance);
				}
				else
				{
					switch(batcher->load_glyph_verts(ret.codepoint))
					{
					case FONT_RESULT::NOT_FOUND:
						serrf("%s glyph not found: U+%X\n", __func__, ret.codepoint);
						return false;
					case FONT_RESULT::ERROR: return false;
					case FONT_RESULT::SUCCESS: break;
					}
				}
			}
		}
		if(currently_selected)
		{
			// end of row, finish the selection
			ASSERT(selection_vertex_buffer_index != -1);
			float pos_x = cull_box() ? std::max(box_xmin, selection_minx) : selection_minx;
			float pos_w =
				cull_box() ? std::min(box_xmax, batcher->draw_x_pos()) : batcher->draw_x_pos();
			float pos_y = batcher->draw_y_pos();
			float pos_h = batcher->draw_y_pos() + lineskip;
			batcher->move_rect(selection_vertex_buffer_index, pos_x, pos_w, pos_y, pos_h);
			selection_vertex_buffer_index = 0;
		}

		if(cur != end)
		{
			// move to the next line.
			batcher->Newline();

			if(cull_box() && batcher->draw_y_pos() >= box_ymax)
			{
				break;
			}

			if(currently_selected)
			{
				// prepare the next row's selection
				selection_minx = batcher->draw_x_pos();
				batcher->set_color(active_selection_color);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				selection_vertex_buffer_index = batcher->font_vertex_buffer.size();
				batcher->draw_rect(0, 0, 0, 0);
				batcher->set_color(select_text_color);
			}
		}
	}

	if(cur == end && static_cast<int>(end) == stb_state.cursor)
	{
		// This needs to be done because stb's layout function will not
		// be called again if there is a newline at the end,
		// because a empty layout would return num_chars = 0, leading to inf loop
		if(!text_data.empty() && text_data.back().codepoint == '\n')
		{
			batcher->Newline();
		}

		if(cull_box() && batcher->draw_y_pos() > box_ymax)
		{
		}
		else
		{
			// cursor at the end of the line
			if(caret_x != NULL) *caret_x = batcher->draw_x_pos();
			if(caret_y != NULL) *caret_y = batcher->draw_y_pos();
			if(caret_visible != NULL) *caret_visible = true;
		}
	}
	return true;
}

bool text_prompt_wrapper::internal_draw_marked(float x, float y)
{
	ASSERT(text_focus);
	ASSERT(!read_only());

	float lineskip = batcher->GetLineSkip();

	// reserve space to draw a backdrop,
	// uses inverted color of the font, except for the alpha
	batcher->set_color(
		255 - text_color[0], 255 - text_color[1], 255 - text_color[2], text_color[3]);
	size_t marked_vertex_buffer_index = batcher->font_vertex_buffer.size();
	batcher->draw_rect(0, 0, 0, 0);
	batcher->set_color(text_color);

	batcher->SetXY(x, y);
	// draw the text
	auto str_cur = markedText.begin();
	auto str_end = markedText.end();
	float marked_caret_x = -1;
	float marked_caret_y = -1;

	// TODO(dootsie): I haven't implemented marked_cursor_end IME selection
	// because I can't test it since I don't have wayland
	marked_caret_x = batcher->draw_x_pos();
	marked_caret_y = batcher->draw_y_pos();

	while(str_cur != str_end)
	{
#ifdef IME_TEXTEDIT_EXT
		if(draw_caret && std::distance(markedText.begin(), str_cur) == marked_cursor_begin)
		{
			marked_caret_x = batcher->draw_x_pos();
			marked_caret_y = batcher->draw_y_pos();
		}
#endif

		uint32_t codepoint;
		utf8::internal::utf_error err_code =
			utf8::internal::validate_next(str_cur, str_end, codepoint);
		if(err_code != utf8::internal::UTF8_OK)
		{
			serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
			return false;
		}

		switch(batcher->load_glyph_verts(codepoint))
		{
		case FONT_RESULT::NOT_FOUND:
			serrf("%s glyph not found: U+%X\n", __func__, codepoint);
			return false;
		case FONT_RESULT::ERROR: return false;
		case FONT_RESULT::SUCCESS: break;
		}
	}
#ifdef IME_TEXTEDIT_EXT
	if(std::distance(markedText.begin(), str_cur) == marked_cursor_begin)
	{
		// get the caret at the end
		marked_caret_x = batcher->draw_x_pos();
		marked_caret_y = batcher->draw_y_pos();
	}
#endif

	float pos_x = x;
	float pos_w = batcher->draw_x_pos();
	float pos_y = y;
	float pos_h = y + lineskip;

	// check if the area is within bounds
	// Keep the camera in bounds
	if(pos_w > box_xmax)
	{
		float x_off = pos_w - box_xmax;
		size_t size = batcher->font_vertex_buffer.size();
		for(; batcher->newline_cursor < size; ++batcher->newline_cursor)
		{
			batcher->font_vertex_buffer[batcher->newline_cursor].pos[0] -= x_off;
		}
		pos_x -= x_off;
		pos_w -= x_off;
		marked_caret_x -= x_off;
	}

	// finish the backdrop
	batcher->move_rect(marked_vertex_buffer_index, pos_x, pos_w, pos_y, pos_h);

	// draw the cursor
	if(draw_caret)
	{
		batcher->set_color(caret_color);
		batcher->draw_rect(
			marked_caret_x, marked_caret_x + 2, marked_caret_y, marked_caret_y + lineskip);
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
			// NOTE: this shouldn't be here because it makes this function only used for clicking
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
			// NOTE: this shouldn't be here because it makes this function only used for clicking
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
	ASSERT(batcher != NULL);

	SDL_Keycode key_shift_mod = ((e.key.keysym.mod & KMOD_SHIFT) != 0 ? STB_TEXTEDIT_K_SHIFT : 0);
	SDL_Keycode key_ctrl_mod = ((e.key.keysym.mod & KMOD_CTRL) != 0 ? STB_TEXTEDIT_K_CONTROL : 0);
	switch(e.type)
	{
	// lazy scroll
	case SDL_MOUSEWHEEL:
		if(y_scrollable())
		{
			// only scroll when the mouse is currently hovering over the bounding box
			int x;
			int y;
			if(SDL_GetMouseState(&x, &y) != 0)
			{
				slogf("info: Failed to get mouse state! SDL Error: %s\n", SDL_GetError());
				break;
			}
			float mouse_x = static_cast<float>(x);
			float mouse_y = static_cast<float>(y);
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				scroll_y -= static_cast<float>(e.wheel.y * cv_prompt_scroll_rate.data) *
							batcher->GetLineSkip();
				// the draw function will clamp it to keep the scroll area inside of the text.
				update_buffer = true;
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
			internal_scroll_y_to(mouse_y);
			update_buffer = true;
		}
		if(x_scrollbar_held)
		{
			internal_scroll_x_to(mouse_x);
			update_buffer = true;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT)
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
				return TEXT_PROMPT_RESULT::EAT;
			}

			if(y_scrollable() && y_scrollbar_held)
			{
				y_scrollbar_held = false;
				internal_scroll_y_to(mouse_y);
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}
			if(x_scrollable() && x_scrollbar_held)
			{
				x_scrollbar_held = false;
				internal_scroll_x_to(mouse_x);
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}

			// don't let this event leak if in focus because the only way to unfocus
			// should be from a button down.
			if(text_focus)
			{
				return TEXT_PROMPT_RESULT::EAT;
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			if(y_scrollable() && internal_scroll_y_inside(mouse_x, mouse_y))
			{
				y_scrollbar_held = true;
				internal_scroll_y_to(mouse_y);
				// if the text was unfocused
				if(!read_only())
				{
					SDL_StartTextInput();
				}
				text_focus = true;
				blink_timer = timer_now();
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}

			if(x_scrollable() && internal_scroll_x_inside(mouse_x, mouse_y))
			{
				x_scrollbar_held = true;
				internal_scroll_x_to(mouse_x);
				// if the text was unfocused
				if(!read_only())
				{
					SDL_StartTextInput();
				}
				text_focus = true;
				blink_timer = timer_now();
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}

			// NOTE: I could make it so that if you click in the empty part of the scrollbar, it
			// moves it, but I don't know if I want that.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				if(!read_only())
				{
					SDL_StartTextInput();
				}
				text_focus = true;
				scroll_to_cursor = true;

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

				blink_timer = timer_now();
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}
			if(!read_only())
			{
				SDL_StopTextInput();
			}
			text_focus = false;
			mouse_held = false;
			drag_x = -1;
			drag_y = -1;
			update_buffer = true;
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
				serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
				return TEXT_PROMPT_RESULT::ERROR;
			}
			wstr.push_back(codepoint);
		}

		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		stb_textedit_paste(this, &stb_state, wstr.data(), wstr.size());

		// clean IME text if it exists.
		markedText.clear();

		blink_timer = timer_now();
		update_buffer = true;
		return TEXT_PROMPT_RESULT::EAT;
	}

	// this might just be a linux problem, but on version 2.0.0
	// it wont send a event with "" for clearing the text editing
	// when you backspace the IME to go away.
	// not sure which version it gets fixed.
	case SDL_TEXTEDITING:
		ASSERT(!read_only());
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

		blink_timer = timer_now();
		update_buffer = true;
		return TEXT_PROMPT_RESULT::EAT;

#ifdef IME_TEXTEDIT_EXT
		// requires SDL_HINT_IME_SUPPORT_EXTENDED_TEXT
		// this actually properly uses the cursor and selection because
		// the old version had a limited string length.
		// in old SDL the start + length was used for splitting the events into chunks
	case SDL_TEXTEDITING_EXT:
		ASSERT(text_focus);
		ASSERT(!read_only());
		if(read_only())
		{
			break;
		}
		slogf(
			"Keyboard: text editExt \"%s\", start: %d, length: %d\n",
			e.editExt.text,
			e.editExt.start,
			e.editExt.length);

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

		blink_timer = timer_now();
		update_buffer = true;

		return TEXT_PROMPT_RESULT::EAT;
#endif
	case SDL_KEYDOWN:
		if(!text_focus)
		{
			break;
		}
		mouse_held = false;
		switch(e.key.keysym.sym)
		{
		// Handle backspace
		case SDLK_BACKSPACE:
			if(read_only())
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_BACKSPACE);
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		case SDLK_RETURN:
			if(read_only())
			{
				break;
			}
			if(single_line())
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, '\n');
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		case SDLK_TAB:
			if(read_only())
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, '\t');
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		// copy
		case SDLK_c:
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				if(STB_TEXT_HAS_SELECTION(&stb_state))
				{
					auto start = text_data.begin() +
								 std::min(stb_state.select_start, stb_state.select_end);
					auto end = text_data.begin() +
							   std::max(stb_state.select_start, stb_state.select_end);
					std::string out;
					for(; start != end; ++start)
					{
						if(!cpputf_append_string(out, start->codepoint))
						{
							ASSERT(false && "invalid codepoint from prompt???");
						}
					}
					if(SDL_SetClipboardText(out.c_str()) != 0)
					{
						slogf("info: Failed to set clipboard! SDL Error: %s\n", SDL_GetError());
						break;
					}
				}
				return TEXT_PROMPT_RESULT::EAT;
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
					auto start = text_data.begin() +
								 std::min(stb_state.select_start, stb_state.select_end);
					auto end = text_data.begin() +
							   std::max(stb_state.select_start, stb_state.select_end);
					std::string out;
					for(; start != end; ++start)
					{
						if(!cpputf_append_string(out, start->codepoint))
						{
							ASSERT(false && "invalid utf8 from prompt???");
						}
					}
					if(stb_textedit_cut(this, &stb_state) != 1)
					{
						slog("info: didn't cut\n");
					}
					update_buffer = true;

					if(SDL_SetClipboardText(out.c_str()) != 0)
					{
						slogf("Failed to set clipboard! SDL Error: %s\n", SDL_GetError());
					}
				}
				return TEXT_PROMPT_RESULT::EAT;
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
				std::unique_ptr<char[], decltype(sdl_del)> utext{SDL_GetClipboardText(), sdl_del};

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
				return TEXT_PROMPT_RESULT::EAT;
			}
			break;
		// arrows
		case SDLK_LEFT:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(
				this, &stb_state, STB_TEXTEDIT_K_LEFT | key_shift_mod | key_ctrl_mod);
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		case SDLK_RIGHT:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(
				this, &stb_state, STB_TEXTEDIT_K_RIGHT | key_shift_mod | key_ctrl_mod);
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		case SDLK_UP:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			// I need to push back to fix a bug when the cursor is at
			// the end of the text, and it won't move up.
			text_data.push_back({'\n', STB_TEXTEDIT_GETWIDTH_NEWLINE});
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_UP | key_shift_mod);
			text_data.pop_back();
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
		case SDLK_DOWN:
			if(read_only() && (e.key.keysym.mod & KMOD_SHIFT) == 0)
			{
				break;
			}
			scroll_to_cursor = true;
			stb_textedit_key(this, &stb_state, STB_TEXTEDIT_K_DOWN | key_shift_mod);
			blink_timer = timer_now();
			update_buffer = true;
			return TEXT_PROMPT_RESULT::EAT;
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
				return TEXT_PROMPT_RESULT::EAT;
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
				return TEXT_PROMPT_RESULT::EAT;
			}
			break;
		case SDLK_a:
			if((e.key.keysym.mod & KMOD_CTRL) != 0)
			{
				stb_state.select_start = 0;
				stb_state.select_end = STB_TEXTEDIT_STRINGLEN(this);
				blink_timer = timer_now();
				update_buffer = true;
				return TEXT_PROMPT_RESULT::EAT;
			}
			break;
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
			if(!single_line() && !x_scrollable() &&
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
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	row->num_chars = std::distance(start, cur);
	row->x0 = box_xmin - scroll_x;
	row->x1 = total_advance - scroll_x;
	row->ymin = box_ymin - scroll_y;
	row->ymax = box_ymin + batcher->GetLineSkip() - scroll_y;
	row->baseline_y_delta = batcher->GetLineSkip();
}

int text_prompt_wrapper::stb_insert_chars(int index, const STB_TEXTEDIT_CHARTYPE* text, int n)
{
	ASSERT(!read_only());
	ASSERT(batcher != NULL);

	if(static_cast<size_t>(index) > text_data.size())
	{
		// yes this does happen because stb undo/redo is broken
		return 0;
	}

	auto it = text_data.insert(text_data.begin() + index, n, prompt_char{});
	for(int i = 0; i < n; ++i, ++it)
	{
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
			it->advance = batcher->GetAdvance(it->codepoint);
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