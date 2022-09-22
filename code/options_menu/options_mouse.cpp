#include "../global.h"

#include "options_mouse.h"
#include "../app.h"

// for the cvars...
#include "../demo.h"

bool mono_normalized_slider_object::input(SDL_Event& e)
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
			return false;
			// leave is only used for releasing "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
	}

	switch(e.type)
	{
	// I don't want a scroll because if the silder is inside a scrollable area (its not)
	// I accidentally modify the silder when I just want to scroll (which is stupid)
	// case SDL_MOUSEWHEEL:
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		if(slider_held)
		{
			internal_move_to(mouse_x);
			// VALUE HAS CHANGED
			return true;
		}
		// helps unfocus other elements.
		if(internal_slider_inside(mouse_x, mouse_y))
		{
			// eat
			set_event_leave(e);
			return false;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			// float mouse_y = static_cast<float>(e.button.y);
			if(slider_held)
			{
				internal_move_to(mouse_x);
				unfocus();
				// eat
				set_event_unfocus(e);
				// VALUE HAS CHANGED
				return true;
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			if(internal_slider_inside(mouse_x, mouse_y))
			{
				slider_held = true;
				// eat
				set_event_unfocus(e);
				return false;
			}
			// snap the slider to the location clicked.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				slider_thumb_click_offset = box_xmin + slider_thumb_size / 2;
				internal_move_to(mouse_x);
				slider_held = true;
				// eat
				set_event_unfocus(e);
				// VALUE HAS CHANGED
				return true;
			}
			unfocus();
		}
		break;
	}
	return false;
}

void mono_normalized_slider_object::draw_buffer()
{
	ASSERT(font_painter != NULL);
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	// draw the scrollbar bbox
	{
		float xmin = box_xmin;
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
		float thumb_offset =
			static_cast<float>(slider_value) * ((box_xmax - box_xmin) - slider_thumb_size);

		float xmin = box_xmin + thumb_offset;
		float xmax = box_xmin + thumb_offset + slider_thumb_size;
		float ymin = box_ymin;
		float ymax = box_ymax;

		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}
}
void mono_normalized_slider_object::unfocus()
{
	slider_held = false;
	slider_thumb_click_offset = -1;
}
void mono_normalized_slider_object::resize_view(float xmin, float xmax, float ymin, float ymax)
{
	box_xmin = xmin;
	box_xmax = xmax;
	box_ymin = ymin;
	box_ymax = ymax;
}
bool mono_normalized_slider_object::internal_slider_inside(float mouse_x, float mouse_y)
{
	float thumb_offset =
		static_cast<float>(slider_value) * ((box_xmax - box_xmin) - slider_thumb_size);
	float xmin = box_xmin + thumb_offset;
	float xmax = box_xmin + thumb_offset + slider_thumb_size;
	float ymin = box_ymin;
	float ymax = box_ymax;

	if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
	{
		slider_thumb_click_offset = mouse_x - thumb_offset;
		return true;
	}

	return false;
}
void mono_normalized_slider_object::internal_move_to(float mouse_x)
{
	// NOTE: I am not sure if float -> double is bad, I know it is, but how bad is it?
	slider_value =
		(mouse_x - slider_thumb_click_offset) / ((box_xmax - box_xmin) - slider_thumb_size);

	// clamp
	slider_value = std::max(0.0, std::min(1.0, slider_value));
}

bool options_mouse_state::init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
{
	ASSERT(font_painter_ != NULL);

	font_painter = font_painter_;
	gl_options_interleave_vbo = vbo;
	gl_options_vao_id = vao;

	invert_text = "invert mouse";
	invert_button.init(font_painter);
	invert_button.text = cv_mouse_invert.data == 1 ? "on" : "off";

	mouse_sensitivity_text = "mouse speed";
	mouse_sensitivity_slider.init(font_painter, cv_mouse_sensitivity.data);

	if(!mouse_sensitivity_prompt.init(
		   std::to_string(cv_mouse_sensitivity.data),
		   font_painter->state.batcher,
		   font_painter->state.font,
		   TEXTP_SINGLE_LINE | TEXTP_X_SCROLL | TEXTP_DRAW_BBOX | TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

	// footer buttons
	revert_button.init(font_painter);
	revert_button.text = "revert";
	revert_button.disabled = true;

	ok_button.init(font_painter);
	ok_button.text = "ok";

	defaults_button.init(font_painter);
	defaults_button.text = "set defaults";

	resize_view();

	return true;
}

OPTIONS_MOUSE_RESULT options_mouse_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: resize_view(); break;
		}
	}

	switch(invert_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		if(previous_invert_value == -1)
		{
			previous_invert_value = cv_mouse_invert.data;
			revert_button.disabled = false;
		}
		cv_mouse_invert.data = (cv_mouse_invert.data == 1) ? 0 : 1;
		invert_button.text = cv_mouse_invert.data == 1 ? "on" : "off";
		break;
	case BUTTON_RESULT::ERROR: return OPTIONS_MOUSE_RESULT::ERROR;
	}

	// mouse speed
	{
		double prev_value = mouse_sensitivity_slider.slider_value;
		if(mouse_sensitivity_slider.input(e))
		{
			if(isnan(previous_mouse_sensitivity_value))
			{
				previous_mouse_sensitivity_value = prev_value;
				revert_button.disabled = false;
			}
			// TODO: convert range
			cv_mouse_sensitivity.data = mouse_sensitivity_slider.slider_value;
			mouse_sensitivity_prompt.replace_string(
				std::to_string(mouse_sensitivity_slider.slider_value));
		}
		switch(mouse_sensitivity_prompt.input(e))
		{
		case TEXT_PROMPT_RESULT::CONTINUE: break;
		case TEXT_PROMPT_RESULT::ERROR: return OPTIONS_MOUSE_RESULT::ERROR;
		}
		if(mouse_sensitivity_prompt.text_focus)
		{
			if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
			{
				std::istringstream iss(mouse_sensitivity_prompt.get_string());
				double out;
				if(!(iss >> out))
				{
					slog("info: failed to convert number.\n");
					mouse_sensitivity_prompt.replace_string(
						std::to_string(cv_mouse_sensitivity.data));
				}
				else
				{
					if(isnan(previous_mouse_sensitivity_value))
					{
						previous_mouse_sensitivity_value = mouse_sensitivity_slider.slider_value;
						revert_button.disabled = false;
					}
					// TODO: convert range (actually just fix the damn copy paste)
					cv_mouse_sensitivity.data = out;
					mouse_sensitivity_slider.slider_value = out;
					mouse_sensitivity_prompt.replace_string(std::to_string(out));
				}
				// eat
				set_event_unfocus(e);
				return OPTIONS_MOUSE_RESULT::CONTINUE;
			}
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
			undo_history();
			return OPTIONS_MOUSE_RESULT::CONTINUE;

		case BUTTON_RESULT::ERROR: return OPTIONS_MOUSE_RESULT::ERROR;
		}

		switch(ok_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("ok click\n");
			close();
			// close acts like an eat
			return OPTIONS_MOUSE_RESULT::CLOSE;
		case BUTTON_RESULT::ERROR: return OPTIONS_MOUSE_RESULT::ERROR;
		}

		switch(defaults_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("reset defaults click\n");
			set_defaults();
			return OPTIONS_MOUSE_RESULT::CONTINUE;
		case BUTTON_RESULT::ERROR: return OPTIONS_MOUSE_RESULT::ERROR;
		}
	}

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		// close counts as an eat.
		return OPTIONS_MOUSE_RESULT::CLOSE;
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
				return OPTIONS_MOUSE_RESULT::CONTINUE;
			}
		}
		break;
	}

	return OPTIONS_MOUSE_RESULT::CONTINUE;
}

bool options_mouse_state::update(double delta_sec)
{
	invert_button.update(delta_sec);

	// footer
	revert_button.update(delta_sec);
	ok_button.update(delta_sec);
	defaults_button.update(delta_sec);

	return true;
}

bool options_mouse_state::draw_text()
{
	float cur_x = box_xmin + element_padding;
	float cur_y = box_ymin + element_padding;

	float button_height = font_painter->state.font->get_lineskip() + font_padding;

	// invert mouse
	font_painter->set_xy(cur_x, cur_y);
	if(!font_painter->draw_text(invert_text.c_str(), invert_text.size()))
	{
		return false;
	}
	cur_y += button_height + element_padding;

	// mouse speed
	font_painter->set_xy(cur_x, cur_y);
	if(!font_painter->draw_text(mouse_sensitivity_text.c_str(), mouse_sensitivity_text.size()))
	{
		// NOLINTNEXTLINE(readability-simplify-boolean-expr)
		return false;
	}
	cur_y += button_height + element_padding;

	return true;
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_mouse_state::render()
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

	// draw the text.
	{
		font_painter->set_anchor(TEXT_ANCHOR::TOP_LEFT);
		font_painter->begin();
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(0, 0, 0, 255);
		if(!draw_text())
		{
			return false;
		}
		font_painter->set_style(FONT_STYLE_NORMAL);
		font_painter->set_color(255, 255, 255, 255);
		if(!draw_text())
		{
			return false;
		}
		font_painter->end();
	}

	// draw buttons
	if(!invert_button.draw_buffer())
	{
		return false;
	}

	// mouse speed
	{
		if(!mouse_sensitivity_prompt.draw())
		{
			return false;
		}
		mouse_sensitivity_slider.draw_buffer();
	}

	// footer buttons
	{
		if(!ok_button.draw_buffer())
		{
			return false;
		}
		if(!revert_button.draw_buffer())
		{
			return false;
		}
		if(!defaults_button.draw_buffer())
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

void options_mouse_state::resize_view()
{
	float footer_height = font_painter->state.font->get_lineskip() + font_padding + element_padding;

	// for a 16px font I would want 400px
	float menu_width = 400 * (font_painter->state.font->get_lineskip() / 16.f);
	float button_height = font_painter->state.font->get_lineskip() + font_padding;

	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	float button_area_height = button_height * static_cast<float>(OPTION_COUNT) +
							   element_padding * static_cast<float>(OPTION_COUNT - 1) +
							   footer_height;

	float x = std::floor((screen_width - menu_width) / 2.f);
	float y = std::floor((screen_height - button_area_height) / 2.f);

	// option objects
	{
		float cur_y = y;
		// invert
		{
			float xmin = (menu_width + element_padding) / 2;
			float xmax = menu_width;
			invert_button.set_rect(x + xmin, cur_y, xmax - xmin, button_height);
			cur_y += button_height + element_padding;
		}

		// mouse speed
		{
			float xmin = (menu_width + element_padding) / 2;
			float xmax = menu_width;
			float prompt_width = 60 * (font_painter->state.font->get_lineskip() / 16.f);
			mouse_sensitivity_prompt.set_bbox(
				x + xmin - element_padding - prompt_width,
				cur_y + font_padding / 2,
				// yuck
				(x + xmin - element_padding) - (x + xmin - element_padding - prompt_width),
				font_painter->state.font->get_lineskip());
			mouse_sensitivity_slider.resize_view(x + xmin, x + xmax, cur_y, cur_y + button_height);
			cur_y += button_height + element_padding;
		}
	}

	// footer buttons
	{
		// for a 16px font I would want 60px
		float button_width = 60 * (font_painter->state.font->get_lineskip() / 16.f);

		float x_cursor = x + menu_width;
		x_cursor -= button_width + element_padding;
		ok_button.set_rect(
			x_cursor,
			y + button_area_height - footer_height + element_padding,
			button_width,
			button_height);
		x_cursor -= button_width + element_padding;
		revert_button.set_rect(
			x_cursor,
			y + button_area_height - footer_height + element_padding,
			button_width,
			button_height);
		// note I double the width here
		x_cursor -= (button_width * 2) + element_padding;
		defaults_button.set_rect(
			x_cursor,
			y + button_area_height - footer_height + element_padding,
			button_width * 2,
			button_height);
	}

	box_xmin = x - element_padding;
	box_xmax = x + menu_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + button_area_height + element_padding;
}

bool options_mouse_state::undo_history()
{
	if(previous_invert_value != -1)
	{
		cv_mouse_invert.data = previous_invert_value;
		invert_button.text = cv_mouse_invert.data == 1 ? "on" : "off";
	}
	if(!isnan(previous_mouse_sensitivity_value))
	{
		cv_mouse_sensitivity.data = previous_mouse_sensitivity_value;
		// TODO convert to range.
		mouse_sensitivity_slider.slider_value = previous_mouse_sensitivity_value;
		mouse_sensitivity_prompt.replace_string(std::to_string(cv_mouse_sensitivity.data));
	}
	clear_history();
	return true;
}

void options_mouse_state::clear_history()
{
	previous_invert_value = -1;
	previous_mouse_sensitivity_value = NAN;
	revert_button.disabled = true;
}

bool options_mouse_state::set_defaults()
{
	// invert
	if(previous_invert_value == -1)
	{
		previous_invert_value = cv_mouse_invert.data;
	}
	if(!cv_mouse_invert.cvar_read(cv_mouse_invert.cvar_default_value.c_str()))
	{
		return false;
	}
	invert_button.text = cv_mouse_invert.data == 1 ? "on" : "off";

	// mouse speed
	if(isnan(previous_mouse_sensitivity_value))
	{
		previous_mouse_sensitivity_value = mouse_sensitivity_slider.slider_value;
	}
	if(!cv_mouse_sensitivity.cvar_read(cv_mouse_sensitivity.cvar_default_value.c_str()))
	{
		return false;
	}
	// TODO convert to range.
	mouse_sensitivity_slider.slider_value = cv_mouse_sensitivity.data;
	mouse_sensitivity_prompt.replace_string(std::to_string(cv_mouse_sensitivity.data));

	// allow revert
	revert_button.disabled = false;

	return true;
}

void options_mouse_state::close()
{
	clear_history();
}