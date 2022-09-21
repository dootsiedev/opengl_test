#include "global.h"

#include "ui.h"

#include "app.h"

void set_event_leave(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_LEAVE;
}

void set_event_unfocus(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
}

void set_event_resize(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
}

void set_event_hidden(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_HIDDEN;
}

BUTTON_RESULT mono_button_object::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
			// release "hover focus"
		case SDL_WINDOWEVENT_LEAVE:
			clicked_on = false;
			hover_over = false;
			return BUTTON_RESULT::CONTINUE;
		case SDL_WINDOWEVENT_HIDDEN:
			// TODO: if a UI element dissapears, use this.
			clicked_on = false;
			hover_over = false;
			fade = 0;
			return BUTTON_RESULT::CONTINUE;

			// there is no "input focus".
			// if you unfocused here, pressing keys in a prompt would
			// make the mouse hover go away!
			// case SDL_WINDOWEVENT_FOCUS_LOST:
		}
	}

	if(disabled)
	{
		return BUTTON_RESULT::CONTINUE;
	}

	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];
		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			hover_over = true;
			// eat
			set_event_leave(e);
			return BUTTON_RESULT::CONTINUE;
		}
		hover_over = false;
	}
	break;
	case SDL_MOUSEBUTTONDOWN: {
		float mouse_x = static_cast<float>(e.button.x);
		float mouse_y = static_cast<float>(e.button.y);

		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];

		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			clicked_on = true;
			// eat
			set_event_leave(e);
			return BUTTON_RESULT::CONTINUE;
		}

		clicked_on = false;
	}
	break;
	case SDL_MOUSEBUTTONUP: {
		if(clicked_on)
		{
			clicked_on = false;

			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			float xmin = button_rect[0];
			float xmax = button_rect[0] + button_rect[2];
			float ymin = button_rect[1];
			float ymax = button_rect[1] + button_rect[3];

			if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
			{
				// slog("click\n");
				// eat
				set_event_unfocus(e);
				return BUTTON_RESULT::TRIGGER;
			}
		}
	}
	break;
	}

	return BUTTON_RESULT::CONTINUE;
}
void mono_button_object::update(double delta_sec)
{
	// NOTE: I wouldn't need this if I used a setter for disabling the button...
	hover_over = !disabled && hover_over;

	// add fade
	fade += static_cast<float>(hover_over ? delta_sec : -delta_sec) * color_state.fade_speed;

	// clamp
	fade = std::min(fade, 1.f);
	fade = std::max(fade, 0.f);
}
bool mono_button_object::draw_buffer()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	// normalize the colors 0-1
	float hot_fill[4] = {
		static_cast<float>(color_state.hot_fill_color[0]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[1]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[2]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[3]) / 255.f};
	float idle_fill[4] = {
		static_cast<float>(color_state.idle_fill_color[0]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[1]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[2]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[3]) / 255.f};

	// blend the colors.
	std::array<uint8_t, 4> fill_color = {
		static_cast<uint8_t>((hot_fill[0] * fade + idle_fill[0] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[1] * fade + idle_fill[1] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[2] * fade + idle_fill[2] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[3] * fade + idle_fill[3] * (1.f - fade)) * 255.f),
	};

	// backdrop
	{
		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];

		// fill
		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);

		// bbox
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, color_state.bbox_color);
	}

	// font
	font_painter->begin();

	if(color_state.show_outline)
	{
		// outline
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(color_state.text_outline_color);
		font_painter->set_anchor(TEXT_ANCHOR::CENTER_PERFECT);
		font_painter->set_xy(
			button_rect[0] + (button_rect[2] / 2.f), button_rect[1] + (button_rect[3] / 2.f));
		if(!font_painter->draw_text(text.c_str(), text.size()))
		{
			return false;
		}
	}

	font_painter->set_style(FONT_STYLE_NORMAL);
	font_painter->set_color(disabled ? color_state.disabled_text_color : color_state.text_color);
	font_painter->set_xy(
		button_rect[0] + (button_rect[2] / 2.f), button_rect[1] + (button_rect[3] / 2.f));
	if(!font_painter->draw_text(text.c_str(), text.size()))
	{
		return false;
	}

	font_painter->end();

	// there are not much gl calls here, but the text does modify the atlas.
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
//
// scrollable
//
void mono_y_scrollable_area::unfocus()
{
	y_scrollbar_held = false;
	scroll_thumb_click_offset = -1;
}
void mono_y_scrollable_area::resize_view(float xmin, float xmax, float ymin, float ymax)
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

void mono_y_scrollable_area::input(SDL_Event& e)
{
	ASSERT(font_painter != NULL);

	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
			// release scrollbar focus.
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			unfocus();
			return;
			// leave is only used for releasing "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
	}

	if(content_h > (box_ymax - box_ymin))
	{
		switch(e.type)
		{
		// lazy scroll
		case SDL_MOUSEWHEEL: {
			// only scroll when the mouse is currently hovering over the bounding box
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			float mouse_x = static_cast<float>(x);
			float mouse_y = static_cast<float>(y);

			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				scroll_y -= static_cast<float>(e.wheel.y * cv_scroll_speed.data) *
							font_painter->state.font->get_lineskip();
				// clamp
				scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
			}
		}
		break;
		case SDL_MOUSEMOTION: {
			float mouse_x = static_cast<float>(e.motion.x);
			float mouse_y = static_cast<float>(e.motion.y);
			if(y_scrollbar_held)
			{
				internal_scroll_y_to(mouse_y);
			}
			// helps unfocus other elements.
			if(internal_scroll_y_inside(mouse_x, mouse_y))
			{
				// eat
				set_event_leave(e);
				return;
			}
		}
		break;
		case SDL_MOUSEBUTTONUP:
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				// float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);
				if(y_scrollbar_held)
				{
					internal_scroll_y_to(mouse_y);
					y_scrollbar_held = false;
					scroll_thumb_click_offset = -1;
					// eat
					set_event_unfocus(e);
					return;
				}
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);

				if(internal_scroll_y_inside(mouse_x, mouse_y))
				{
					y_scrollbar_held = true;
					// eat
					set_event_unfocus(e);
					return;
				}

				y_scrollbar_held = false;
				scroll_thumb_click_offset = -1;
				// unfocus();
			}
			break;
		}
	}
}

void mono_y_scrollable_area::draw_buffer()
{
	ASSERT(font_painter != NULL);
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	if(content_h > (box_ymax - box_ymin))
	{
		// draw the scrollbar bbox
		{
			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin;
			float ymax = box_ymax;

			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
		// draw the scrollbar thumb
		{
			float scrollbar_max_height = (box_ymax - box_ymin);

			float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
			thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

			float scroll_ratio =
				(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
			float thumb_offset = scroll_y * scroll_ratio;

			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin + thumb_offset;
			float ymax = box_ymin + thumb_offset + thumb_height;

			batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
	}
}

bool mono_y_scrollable_area::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio =
		(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
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

	return false;
}

void mono_y_scrollable_area::internal_scroll_y_to(float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio =
		(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
	scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;

	// clamp
	scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
}

//
// prompt
//

#if 0

void simple_prompt_state::init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
{
	ASSERT(font_painter_ != NULL);

	font_painter = font_painter_;
	gl_options_interleave_vbo = vbo;
	gl_options_vao_id = vao;

	{
		select_entry& entry = select_entries.emplace_back();
		entry.button.init(font_painter);
		entry.button.text = "Video";
		entry.result = OPTIONS_SELECT_RESULT::OPEN_VIDEO;
	}
	{
		select_entry& entry = select_entries.emplace_back();
		entry.button.init(font_painter);
		entry.button.text = "Controls";
		entry.result = OPTIONS_SELECT_RESULT::OPEN_CONTROLS;
	}
	{
#ifdef AUDIO_SUPPORT
		select_entry& entry = select_entries.emplace_back();
		entry.button.init(font_painter);
		entry.button.text = "Audio";
		entry.result = OPTIONS_SELECT_RESULT::OPEN_AUDIO;
#endif
	}
	{
		select_entry& entry = select_entries.emplace_back();
		entry.button.init(font_painter);
		entry.button.text = "Done";
		entry.result = OPTIONS_SELECT_RESULT::CLOSE;
	}
	resize_view();
}

OPTIONS_SELECT_RESULT simple_prompt_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: resize_view(); break;
		}
	}

	for(auto& entry : select_entries)
	{
		switch(entry.button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			if(entry.result == OPTIONS_SELECT_RESULT::CLOSE)
			{
				// eat
				set_event_unfocus(e);
			}
			return entry.result;
		case BUTTON_RESULT::ERROR: return OPTIONS_SELECT_RESULT::ERROR;
		}
	}

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		unfocus();
		return OPTIONS_SELECT_RESULT::CLOSE;
	}

	// backdrop
	switch(e.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			unfocus();
		}
		[[fallthrough]];
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			// helps unfocus other elements.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				// eat
				set_event_unfocus(e);
				return OPTIONS_SELECT_RESULT::CONTINUE;
			}
		}
		break;
	}

	return OPTIONS_SELECT_RESULT::CONTINUE;
}

bool simple_prompt_state::update(double delta_sec)
{
	for(auto& entry : select_entries)
	{
		if(!entry.button.update(delta_sec))
		{
			return false;
		}
	}
	return true;
}

bool simple_prompt_state::render()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	batcher->clear();

	// draw the backdrop bbox
	{
		float xmin = box_xmin;
		float xmax = box_xmax;
		float ymin = box_ymin;
		float ymax = box_ymax;

		std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}

	// draw buttons
	for(auto& entry : select_entries)
	{
		if(!entry.button.draw_buffer())
		{
			return false;
		}
	}

	if(batcher->get_quad_count() != 0)
	{
		// upload
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
		ctx.glBufferData(GL_ARRAY_BUFFER, batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
		ctx.glBufferSubData(
			GL_ARRAY_BUFFER, 0, batcher->get_current_vertex_size(), batcher->buffer);
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

		// draw
		ctx.glBindVertexArray(gl_options_vao_id);
		ctx.glDrawArrays(GL_TRIANGLES, 0, batcher->get_current_vertex_count());
		ctx.glBindVertexArray(0);
	}
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void simple_prompt_state::resize_view()
{
	// for a 16px font I would want 200px
	float button_width = 200 * (font_painter->state.font->get_point_size() / 16.f);
	float button_height = font_painter->state.font->get_point_size() + font_padding;

	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	float button_area_height = button_height * static_cast<float>(select_entries.size()) +
							   element_padding * static_cast<float>(select_entries.size() - 1);

	float x = std::floor((screen_width - button_width) / 2.f);
	float y = std::floor((screen_height - button_area_height) / 2.f);

	float cur_y = y;
	for(auto& entry : select_entries)
	{
		entry.button.set_rect({x, cur_y, button_width, button_height});
		cur_y += button_height + element_padding;
	}

	box_xmin = x - element_padding;
	box_xmax = x + button_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + button_area_height + element_padding;
}

void simple_prompt_state::unfocus()
{
	for(auto& entry : select_entries)
	{
		entry.button.unfocus();
	}
}
#endif