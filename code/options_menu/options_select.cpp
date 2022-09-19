#include "../global.h"

#include "options_select.h"

#include "../app.h"

// TODO(dootsie): make the escape button close the menu,
// and make a popup that asks if you want to keep the changes?
// TODO(dootsie): each keybind should have it's own "revert to default".

void options_select_state::init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
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

OPTIONS_SELECT_RESULT options_select_state::input(SDL_Event& e)
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

bool options_select_state::update(double delta_sec)
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

bool options_select_state::render()
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

void options_select_state::resize_view()
{
	// for a 16px font I would want 200px
	float button_width = 200 * (font_painter->state.font->get_lineskip() / 16.f);
	float button_height = font_painter->state.font->get_lineskip() + font_padding;

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

void options_select_state::unfocus()
{
	for(auto& entry : select_entries)
	{
		entry.button.unfocus();
	}
}