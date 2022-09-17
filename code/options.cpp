#include "global.h"

#include "options.h"
#include "app.h"

// TODO(dootsie): make the escape button close the menu, but make a popup that asks if you want to
// keep the changes.

void mono_button_object::init(font_sprite_painter* font_painter_, button_color_state* color_state_)
{
	ASSERT(font_painter_ != NULL);

	font_painter = font_painter_;
	if(color_state_ != NULL)
	{
		color_state = *color_state_;
	}
}
BUTTON_RESULT mono_button_object::input(SDL_Event& e)
{
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

		hover_over = ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x;
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
			if(mouse_button_down)
			{
				// slog("click\n");
				return BUTTON_RESULT::TRIGGER;
			}
			clicked_on = true;
		}
		else
		{
			clicked_on = false;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP: {
		if(!mouse_button_down && clicked_on)
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
				return BUTTON_RESULT::TRIGGER;
			}
		}
	}
	break;
	}

	return BUTTON_RESULT::CONTINUE;
}
bool mono_button_object::update(double delta_sec)
{
    // NOTE: I wouldn't need this if I used a setter for disabling the button...
	hover_over = !disabled && hover_over;

	// add fade
	fade += static_cast<float>(hover_over ? delta_sec : -delta_sec) * color_state.fade_speed;

	// clamp
	fade = std::min(fade, 1.f);
	fade = std::max(fade, 0.f);

	return true;
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

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
void mono_button_object::unfocus()
{
    clicked_on = false;
	hover_over = false;
}

bool option_menu_state::init(
	font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader)
{
	ASSERT(font_ != NULL);
	ASSERT(batcher_ != NULL);

	font_painter.init(batcher_, font_);

	footer_height = font_->get_lineskip() + font_padding + element_padding;

	for(const auto& [key, value] : get_keybinds())
	{
		switch(value.visablity)
		{
		case KEYBIND_VIS::HIDDEN: break;
		case KEYBIND_VIS::NORMAL:
			buttons.emplace_back(value);
			buttons.back().button.init(&font_painter);
			buttons.back().button.text = value.cvar_write();
			break;
		}
	}

	revert_button.init(&font_painter);
	revert_button.text = "revert";
	revert_button.disabled = true;

	ok_button.init(&font_painter);
	ok_button.text = "ok";

	defaults_button.init(&font_painter);
	defaults_button.text = "reset defaults";

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_options_interleave_vbo);
	if(gl_options_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// VAO
	ctx.glGenVertexArrays(1, &gl_options_vao_id);
	if(gl_options_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	// vertex setup
	ctx.glBindVertexArray(gl_options_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	ctx.glBindVertexArray(0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool option_menu_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_options_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_options_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

OPTION_MENU_RESULT option_menu_state::input(SDL_Event& e)
{
	/*
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: draw_requested = true; break;
		}
	}*/

	std::array<float, 4> scroll_view = internal_get_scrollbox_view();
	float scroll_xmin = scroll_view[0];
	float scroll_xmax = scroll_view[0] + scroll_view[2];
	float scroll_ymin = scroll_view[1];
	float scroll_ymax = scroll_view[1] + scroll_view[3];

	// scrollbar input
	if(scroll_h > scroll_view[3])
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

			if(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
			   scroll_xmin <= mouse_x)
			{
				scroll_y -= static_cast<float>(e.wheel.y * cv_scroll_speed.data) *
							font_painter.state.font->get_lineskip();
				// clamp
				scroll_y = std::max(0.f, std::min(scroll_h - scroll_view[3], scroll_y));
			}
		}
		break;
		case SDL_MOUSEMOTION: {
			float mouse_y = static_cast<float>(e.motion.y);
			if(y_scrollbar_held)
			{
				internal_scroll_y_to(mouse_y);
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
					scroll_drag_y = -1;
					scroll_thumb_click_offset = -1;
					return OPTION_MENU_RESULT::EAT;
				}

				// helps unfocus other elements.
				/*if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
				box_xmin <= mouse_x)
				{
					return OPTION_MENU_RESULT::EAT;
				}*/
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
					internal_scroll_y_to(mouse_y);
					return OPTION_MENU_RESULT::EAT;
				}

				y_scrollbar_held = false;
				scroll_drag_y = -1;
				scroll_thumb_click_offset = -1;
				// unfocus();
			}
			break;
		}
	}

	if(requested_button != NULL)
	{
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
				return OPTION_MENU_RESULT::ERROR;
			}
			button.text = keybind.cvar_write();
			button.color_state.text_color = {255, 255, 255, 255};
			requested_button = NULL;

			//slogf("%s = %s\n", keybind.cvar_comment, keybind.cvar_write().c_str());
			return OPTION_MENU_RESULT::EAT;
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

			//slogf("%s = %s\n", keybind.cvar_comment, keybind.cvar_write().c_str());
			return OPTION_MENU_RESULT::EAT;
		}
	}

	// filter out mouse events that are clipped out of the scroll_view
	bool in_scroll_bounds = true;
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		if(!(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
			 scroll_xmin <= mouse_x))
		{
			in_scroll_bounds = false;
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
				in_scroll_bounds = false;
			}
			break;
		}
	}

	if(!in_scroll_bounds)
	{
		for(auto& button : buttons)
		{
			button.button.unfocus();
		}
	}
	else
	{
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
				requested_button = &button;
				button.button.text = "[press button]";
				button.button.color_state.text_color = {255, 255, 0, 255};
				return OPTION_MENU_RESULT::EAT;
			case BUTTON_RESULT::ERROR: return OPTION_MENU_RESULT::ERROR;
			}
		}
	}

	// footer buttons
	{
		switch(revert_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			//slog("revert click\n");
			for(auto rit = history.rbegin(); rit != history.rend(); ++rit)
			{
				rit->slot.keybind.key_binds = rit->value;
				rit->slot.button.text = rit->slot.keybind.cvar_write();
			}
			history.clear();
			revert_button.disabled = true;
			return OPTION_MENU_RESULT::EAT;

		case BUTTON_RESULT::ERROR: return OPTION_MENU_RESULT::ERROR;
		}

		switch(ok_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			//slog("ok click\n");
			history.clear();
			revert_button.disabled = true;
			return OPTION_MENU_RESULT::CLOSE;
		case BUTTON_RESULT::ERROR: return OPTION_MENU_RESULT::ERROR;
		}

		switch(defaults_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			//slog("reset defaults click\n");
			for(auto& button : buttons)
			{
				history.emplace_back(button.keybind.key_binds, button);
				if(!button.keybind.cvar_read(button.keybind.cvar_default_value.c_str()))
				{
					return OPTION_MENU_RESULT::ERROR;
				}
				button.button.text = button.keybind.cvar_write();
			}
			revert_button.disabled = false;
			return OPTION_MENU_RESULT::EAT;
		case BUTTON_RESULT::ERROR: return OPTION_MENU_RESULT::ERROR;
		}
	}

	if( //! input_eaten &&
		e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		// unfocus any selection left.
		// not an eat because this is similar to clicking outside the element
		unfocus();
	}

	// backdrop
	switch(e.type)
	{
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			// helps unfocus other elements.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				return OPTION_MENU_RESULT::EAT;
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			// helps unfocus other elements.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				return OPTION_MENU_RESULT::EAT;
			}

			unfocus();
		}
		break;
	}

	return OPTION_MENU_RESULT::CONTINUE;
}

bool option_menu_state::draw_base()
{
	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	float button_height = font_painter.state.font->get_lineskip() + font_padding;

	// draw the backdrop bbox
	{
		float xmin = box_xmin - element_padding;
		float xmax = box_xmax + element_padding;
		float ymin = box_ymin - element_padding;
		float ymax = box_ymax + element_padding;

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
		float button_width = 60 * (font_painter.state.font->get_lineskip() / 16.f);

		float x_cursor = box_xmax;
		x_cursor -= button_width;
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

bool option_menu_state::draw_scroll()
{
	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
	std::array<float, 4> scroll_view = internal_get_scrollbox_view();
	// float scroll_xmin = scroll_view[0];
	float scroll_xmax = scroll_view[0] + scroll_view[2];
	float scroll_ymin = scroll_view[1];
	float scroll_ymax = scroll_view[1] + scroll_view[3];

	float button_height = font_painter.state.font->get_lineskip() + font_padding;

	// set the buttons dimensions
	{
		float x = (scroll_view[2] - element_padding) / 2 + element_padding;
		float y = 0;
		float width = scroll_view[2] - scrollbar_thickness - element_padding - x;
		for(auto& entry : buttons)
		{
			entry.button.set_rect(
				scroll_view[0] + x, scroll_view[1] + y - scroll_y, width, button_height);
			y += button_height + element_padding;
		}
		// very important to do this before drawing the scrollbar.
		scroll_h = y - element_padding;
	}

	if(scroll_h > scroll_view[3])
	{
		// draw the scrollbar bbox
		{
			float xmin = scroll_xmax - scrollbar_thickness;
			float xmax = scroll_xmax;
			float ymin = scroll_ymin;
			float ymax = scroll_ymax;

			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
		// draw the scrollbar thumb
		{
			float scrollbar_max_height = scroll_view[3];

			float thumb_height = scrollbar_max_height * (scroll_view[3] / scroll_h);
			thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

			float scroll_ratio =
				(scrollbar_max_height - thumb_height) / (scroll_h - scroll_view[3]);
			float thumb_offset = scroll_y * scroll_ratio;

			float xmin = scroll_xmax - scrollbar_thickness;
			float xmax = scroll_xmax;
			float ymin = scroll_ymin + thumb_offset;
			float ymax = scroll_ymin + thumb_offset + thumb_height;

			std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

			batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
	}

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


		    font_painter.begin();

			// outline
			font_painter.set_style(FONT_STYLE_OUTLINE);
			font_painter.set_color(0, 0, 0, 255);
			font_painter.set_anchor(TEXT_ANCHOR::CENTER_LEFT);
			font_painter.set_xy(
				box_xmin + element_padding, box_ymin + y_pos + button_height / 2.f - scroll_y);
			if(!font_painter.draw_text(button.keybind.cvar_comment, len))
			{
				return false;
			}

			// outline inside
			font_painter.set_style(FONT_STYLE_NORMAL);
			font_painter.set_color(255, 255, 255, 255);
			font_painter.set_xy(
				box_xmin + element_padding, box_ymin + y_pos + button_height / 2.f - scroll_y);

			if(!font_painter.draw_text(button.keybind.cvar_comment, len))
			{
				return false;
			}
		    font_painter.end();


            if(!button.button.draw_buffer())
            {
                return false;
            }
		}
	}
	return true;
}

bool option_menu_state::update(double delta_sec)
{
	for(auto& button : buttons)
	{
		if(!button.button.update(delta_sec))
		{
			return false;
		}
	}
	if(!revert_button.update(delta_sec))
	{
		return false;
	}
	if(!ok_button.update(delta_sec))
	{
		return false;
	}
	if(!defaults_button.update(delta_sec))
	{
		return false;
	}
	return true;
}

bool option_menu_state::render()
{
	mono_2d_batcher* batcher = font_painter.state.batcher;
	batcher->clear();

	// TODO: fullscreen makes the buttons too big, maybe shrink it?
	box_xmin = 60;
	box_xmax = static_cast<float>(cv_screen_width.data) - 60;
	box_ymin = 60;
	box_ymax = static_cast<float>(cv_screen_height.data) - 60;

	// clamp the scroll (when the screen resizes)
	scroll_y = std::max(0.f, std::min(scroll_h - internal_get_scrollbox_view()[3], scroll_y));

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
		std::array<float, 4> scroll_view = internal_get_scrollbox_view();
		GLint scissor_x = static_cast<GLint>(scroll_view[0]);
		GLint scissor_y = static_cast<GLint>(scroll_view[1]);
		GLint scissor_w = static_cast<GLint>(scroll_view[2]);
		GLint scissor_h = static_cast<GLint>(scroll_view[3]);
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

void option_menu_state::unfocus()
{
	y_scrollbar_held = false;
	scroll_drag_y = -1;
	scroll_thumb_click_offset = -1;

	// all this does is make the buttons not hovered
	// since if you close the menu while hovering a button,
	// when you make the menu re-appear, and if you don't move
	// the mouse, the button will stay "hot".
	for(auto& button : buttons)
	{
		button.button.unfocus();
	}
	ok_button.unfocus();
	revert_button.unfocus();
	defaults_button.unfocus();
}

void option_menu_state::internal_scroll_y_to(float mouse_y)
{
	std::array<float, 4> pos = internal_get_scrollbox_view();
	float scrollbar_max_height = pos[3];

	float thumb_height = scrollbar_max_height * (pos[3] / scroll_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio = (scrollbar_max_height - thumb_height) / (scroll_h - pos[3]);
	scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;

	// clamp
	scroll_y = std::max(0.f, std::min(scroll_h - pos[3], scroll_y));
}

bool option_menu_state::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	std::array<float, 4> pos = internal_get_scrollbox_view();
	float scrollbar_max_height = pos[3];

	float thumb_height = scrollbar_max_height * (pos[3] / scroll_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio = (scrollbar_max_height - thumb_height) / (scroll_h - pos[3]);
	float thumb_offset = scroll_y * scroll_ratio;

	float xmin = pos[0] + pos[2] - scrollbar_thickness;
	float xmax = pos[0] + pos[2];
	float ymin = pos[1] + thumb_offset;
	float ymax = pos[1] + thumb_offset + thumb_height;

	if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
	{
		scroll_thumb_click_offset = mouse_y - thumb_offset;
		return true;
	}

	return false;
}

std::array<float, 4> option_menu_state::internal_get_scrollbox_view() const
{
	std::array<float, 4> out;
	// x
	out[0] = box_xmin;
	// y
	out[1] = box_ymin;
	// w
	out[2] = box_xmax - box_xmin;
	// h
	out[3] = box_ymax - box_ymin - footer_height;
	return out;
}