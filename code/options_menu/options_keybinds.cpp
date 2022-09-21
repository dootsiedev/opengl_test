#include "../global.h"

#include "options_keybinds.h"

#include "../app.h"

// TODO(dootsie): make the escape button close the menu,
// and make a popup that asks if you want to keep the changes?
// TODO(dootsie): each keybind should have it's own "revert to default".

void options_keybinds_state::init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
{
	ASSERT(font_painter_ != NULL);

	font_painter = font_painter_;

	footer_height = font_painter->state.font->get_point_size() + font_padding + element_padding * 2;

	gl_options_interleave_vbo = vbo;
	gl_options_vao_id = vao;

	for(const auto& [key, value] : get_keybinds())
	{
		switch(value.visablity)
		{
		case KEYBIND_VIS::HIDDEN: break;
		case KEYBIND_VIS::NORMAL:
			buttons.emplace_back(value);
			buttons.back().button.init(font_painter);
			buttons.back().button.text = value.cvar_write();
			break;
		}
	}

	revert_button.init(font_painter);
	revert_button.text = "revert";
	revert_button.disabled = true;

	ok_button.init(font_painter);
	ok_button.text = "ok";

	defaults_button.init(font_painter);
	defaults_button.text = "set defaults";

	// scrollbar
	scroll_state.init(font_painter);
	scroll_state.scrollbar_padding = element_padding;

	resize_view();
}

void options_keybinds_state::clear_history()
{
	history.clear();
	revert_button.disabled = true;
}

void options_keybinds_state::close()
{
	clear_history();
	scroll_state.scroll_to_top();
}

OPTIONS_KEYBINDS_RESULT options_keybinds_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: resize_view(); break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			// release requested_button focus.
			if(requested_button != NULL)
			{
				cvar_key_bind& keybind = requested_button->keybind;
				mono_button_object& button = requested_button->button;
				button.text = keybind.cvar_write();
				button.color_state.text_color = {255, 255, 255, 255};
				requested_button = NULL;
			}
			break;
		}
	}

	// button request
	if(requested_button != NULL)
	{
		// TODO: make a popup prompt with a "cancel" button that is priority over the binding.
		// maybe also mention "escape = NONE".

		cvar_key_bind& keybind = requested_button->keybind;
		mono_button_object& button = requested_button->button;

		if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
		{
			// insert into history
			history.emplace_back(keybind.key_binds, *requested_button);
			revert_button.disabled = false;

			// set the button
			if(!keybind.cvar_read("NONE"))
			{
				return OPTIONS_KEYBINDS_RESULT::ERROR;
			}
			button.text = keybind.cvar_write();
			button.color_state.text_color = {255, 255, 255, 255};
			requested_button = NULL;

			// slogf("%s = %s\n", keybind.cvar_comment, keybind.cvar_write().c_str());
			// eat
			set_event_unfocus(e);
			return OPTIONS_KEYBINDS_RESULT::CONTINUE;
		}

		keybind_state out;
		if(keybind.bind_sdl_event(e, &out))
		{
			// insert into history
			history.emplace_back(keybind.key_binds, *requested_button);
			revert_button.disabled = false;

			// set the button
			keybind.key_binds = out;
			button.text = keybind.cvar_write();
			button.color_state.text_color = {255, 255, 255, 255};
			requested_button = NULL;

			// slogf("%s = %s\n", keybind.cvar_comment, keybind.cvar_write().c_str());
			// eat
			set_event_unfocus(e);
			return OPTIONS_KEYBINDS_RESULT::CONTINUE;
		}
	}

	// footer buttons
	{
		switch(revert_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("revert click\n");
			for(auto rit = history.rbegin(); rit != history.rend(); ++rit)
			{
				rit->slot.keybind.key_binds = rit->value;
				rit->slot.button.text = rit->slot.keybind.cvar_write();
			}
			clear_history();
			return OPTIONS_KEYBINDS_RESULT::CONTINUE;

		case BUTTON_RESULT::ERROR: return OPTIONS_KEYBINDS_RESULT::ERROR;
		}

		switch(ok_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("ok click\n");
			close();
			// close acts like an eat
			return OPTIONS_KEYBINDS_RESULT::CLOSE;
		case BUTTON_RESULT::ERROR: return OPTIONS_KEYBINDS_RESULT::ERROR;
		}

		switch(defaults_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("reset defaults click\n");
			for(auto& button : buttons)
			{
				history.emplace_back(button.keybind.key_binds, button);
				if(!button.keybind.cvar_read(button.keybind.cvar_default_value.c_str()))
				{
					return OPTIONS_KEYBINDS_RESULT::ERROR;
				}
				button.button.text = button.keybind.cvar_write();
			}
			revert_button.disabled = false;
			return OPTIONS_KEYBINDS_RESULT::CONTINUE;
		case BUTTON_RESULT::ERROR: return OPTIONS_KEYBINDS_RESULT::ERROR;
		}
	}

	// scroll

	scroll_state.input(e);

	float scroll_xmin = scroll_state.box_xmin;
	float scroll_xmax = scroll_state.box_xmax;
	float scroll_ymin = scroll_state.box_ymin;
	float scroll_ymax = scroll_state.box_ymax;

	// filter out mouse events that are clipped out of the scroll_view
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		if(!(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
			 scroll_xmin <= mouse_x))
		{
			// un hover
			set_event_leave(e);
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			if(!(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
				 scroll_xmin <= mouse_x))
			{
				// eat the mouse
				set_event_unfocus(e);
				return OPTIONS_KEYBINDS_RESULT::CONTINUE;
			}
		}
		break;
	}
	for(auto& button : buttons)
	{
		// too high
		if(scroll_ymin >= button.button.button_rect[1] + button.button.button_rect[3])
		{
			continue;
		}
		// too low
		if(scroll_ymax <= button.button.button_rect[1])
		{
			break;
		}
		switch(button.button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			requested_button = &button;
			button.button.text = "[press button]";
			button.button.color_state.text_color = {255, 255, 0, 255};
			return OPTIONS_KEYBINDS_RESULT::CONTINUE;
		case BUTTON_RESULT::ERROR: return OPTIONS_KEYBINDS_RESULT::ERROR;
		}
	}

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		close();
		// close acts like an eat
		return OPTIONS_KEYBINDS_RESULT::CLOSE;
	}

	// backdrop
	switch(e.type)
	{
	case SDL_MOUSEBUTTONDOWN:
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
				return OPTIONS_KEYBINDS_RESULT::CONTINUE;
			}
		}
	}

	return OPTIONS_KEYBINDS_RESULT::CONTINUE;
}

bool options_keybinds_state::draw_base()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	float button_height = font_painter->state.font->get_point_size() + font_padding;

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

	// the footer buttons
	{
		// for a 16px font I would want 60px
		float button_width = 60 * (font_painter->state.font->get_point_size() / 16.f);

		float x_cursor = box_xmax;
		x_cursor -= button_width + element_padding;
		ok_button.set_rect(
			x_cursor, box_ymax - footer_height + element_padding, button_width, button_height);
		if(!ok_button.draw_buffer())
		{
			return false;
		}
		x_cursor -= button_width + element_padding;
		revert_button.set_rect(
			x_cursor, box_ymax - footer_height + element_padding, button_width, button_height);
		if(!revert_button.draw_buffer())
		{
			return false;
		}
		// note I double the width here
		x_cursor -= (button_width * 2) + element_padding;
		defaults_button.set_rect(
			x_cursor, box_ymax - footer_height + element_padding, button_width * 2, button_height);
		if(!defaults_button.draw_buffer())
		{
			return false;
		}
	}

	return true;
}

bool options_keybinds_state::draw_scroll()
{
	/*
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
	*/

	float scroll_xmin = scroll_state.box_xmin;
	float scroll_xmax = scroll_state.box_inner_xmax;
	float scroll_ymin = scroll_state.box_ymin;
	float scroll_ymax = scroll_state.box_ymax;

	float button_height = font_painter->state.font->get_point_size() + font_padding;

	// set the buttons dimensions
	{
		float y = 0;
		float xmin = ((scroll_xmax - scroll_xmin) - element_padding) / 2;
		float xmax = (scroll_xmax - scroll_xmin);
#if 0
#endif
		//
		// float width = (scroll_xmax - scroll_xmin);
		for(auto& entry : buttons)
		{
			entry.button.set_rect(
				xmin + scroll_xmin,
				scroll_ymin + y - scroll_state.scroll_y,
				xmax - xmin,
				button_height);
			y += button_height + element_padding;
		}
		// very important to do this before drawing the scrollbar.
		scroll_state.content_h = y - element_padding;
	}

	scroll_state.draw_buffer();

	// draw the keybind description text
	{
		float y = 0;
		for(auto& button : buttons)
		{
			float y_pos = y;
			y += button_height + element_padding;

			// too high
			if(scroll_ymin >= button.button.button_rect[1] + button.button.button_rect[3])
			{
				continue;
			}
			// too low
			if(scroll_ymax <= button.button.button_rect[1])
			{
				break;
			}

			size_t len = strlen(button.keybind.cvar_comment);

			font_painter->begin();

			// outline
			font_painter->set_style(FONT_STYLE_OUTLINE);
			font_painter->set_color(0, 0, 0, 255);
			font_painter->set_anchor(TEXT_ANCHOR::CENTER_LEFT);
			font_painter->set_xy(
				scroll_xmin, scroll_ymin + y_pos + button_height / 2.f - scroll_state.scroll_y);
			if(!font_painter->draw_text(button.keybind.cvar_comment, len))
			{
				return false;
			}

			// outline inside
			font_painter->set_style(FONT_STYLE_NORMAL);
			font_painter->set_color(255, 255, 255, 255);
			font_painter->set_xy(
				scroll_xmin, scroll_ymin + y_pos + button_height / 2.f - scroll_state.scroll_y);

			if(!font_painter->draw_text(button.keybind.cvar_comment, len))
			{
				return false;
			}
			font_painter->end();

			if(!button.button.draw_buffer())
			{
				return false;
			}
		}
	}
	return true;
}

bool options_keybinds_state::update(double delta_sec)
{
	for(auto& button : buttons)
	{
		button.button.update(delta_sec);
	}
	revert_button.update(delta_sec);
	ok_button.update(delta_sec);
	defaults_button.update(delta_sec);
	return true;
}

bool options_keybinds_state::render()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	batcher->clear();

	if(!draw_base())
	{
		return false;
	}

	// I need to split the batch into 2 draw calls for clipping the scroll.
	GLsizei base_vertex_count = batcher->get_current_vertex_count();

	if(!draw_scroll())
	{
		return false;
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

		// the base
		ctx.glDrawArrays(GL_TRIANGLES, 0, base_vertex_count);

		// the scroll box
		GLint scissor_x = static_cast<GLint>(scroll_state.box_xmin);
		GLint scissor_y = static_cast<GLint>(scroll_state.box_ymin);
		GLint scissor_w = static_cast<GLint>(scroll_state.box_xmax - scroll_state.box_xmin);
		GLint scissor_h = static_cast<GLint>(scroll_state.box_ymax - scroll_state.box_ymin);
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glDrawArrays(
				GL_TRIANGLES,
				base_vertex_count,
				batcher->get_current_vertex_count() - base_vertex_count);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
		ctx.glBindVertexArray(0);
	}
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void options_keybinds_state::resize_view()
{
	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	// for a 16px font I would want 400px
	float max_width = 400 * (font_painter->state.font->get_point_size() / 16.f);
	float menu_width = std::min(screen_width - 60 * 2, max_width);

	// NOTE: I could also try to make the height have a max size too.
	float menu_height = screen_height - 60 * 2;

	float xmin = std::floor((screen_width - menu_width) / 2.f);
	float ymin = std::floor((screen_height - menu_height) / 2.f);

	box_xmin = xmin;
	box_xmax = xmin + menu_width;
	box_ymin = ymin;
	box_ymax = ymin + menu_height;
	scroll_state.resize_view(
		box_xmin + element_padding,
		box_xmax - element_padding,
		box_ymin + element_padding,
		box_ymax - footer_height);
}