#include "global.h"

#include "options.h"
#include "app.h"

//TODO: add a save, cancel, and apply button?

void mono_button_object::init(font_sprite_painter* font_painter_, button_color_state* color_state_)
{
	ASSERT(font_painter_ != NULL);
	ASSERT(color_state_ != NULL);

	font_painter = font_painter_;
	color_state = color_state_;
}
BUTTON_RESULT mono_button_object::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		float xmin = pos[0];
		float xmax = pos[0] + pos[2];
		float ymin = pos[1];
		float ymax = pos[1] + pos[3];

		hover_over = ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x;
	}
	break;
	case SDL_MOUSEBUTTONDOWN: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		float xmin = pos[0];
		float xmax = pos[0] + pos[2];
		float ymin = pos[1];
		float ymax = pos[1] + pos[3];

		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			slog("click\n");
			return BUTTON_RESULT::TRIGGER;
		}
	}
	break;
	}

	return BUTTON_RESULT::CONTINUE;
}
bool mono_button_object::update(double delta_sec)
{
	// add fade
	fade += static_cast<float>(hover_over ? delta_sec : -delta_sec) * color_state->fade_speed;
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
		static_cast<float>(color_state->hot_fill_color[0]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[1]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[2]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[3]) / 255.f};
	float idle_fill[4] = {
		static_cast<float>(color_state->idle_fill_color[0]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[1]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[2]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[3]) / 255.f};

	// blend the colors.
	std::array<uint8_t, 4> fill_color = {
		static_cast<uint8_t>((hot_fill[0] * fade + idle_fill[0] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[1] * fade + idle_fill[1] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[2] * fade + idle_fill[2] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[3] * fade + idle_fill[3] * (1.f - fade)) * 255.f),
	};

	{
		float xmin = pos[0];
		float xmax = pos[0] + pos[2];
		float ymin = pos[1];
		float ymax = pos[1] + pos[3];

		// fill
		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);

		// bbox
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, color_state->bbox_color);
	}
    // scroll bar
	{
		float xmin = pos[0];
		float xmax = pos[0] + pos[2];
		float ymin = pos[1];
		float ymax = pos[1] + pos[3];

		// fill
		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);

		// bbox
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, color_state->bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, color_state->bbox_color);
	}

	// font
	font_painter->begin();

	// outline
	font_painter->set_style(FONT_STYLE_OUTLINE);
	font_painter->set_color(0, 0, 0, 255);
	font_painter->set_anchor(TEXT_ANCHOR::CENTER_PERFECT);
	font_painter->set_xy(pos[0] + (pos[2] / 2.f), pos[1] + (pos[3] / 2.f));
	if(!font_painter->draw_text(text.c_str(), text.size()))
	{
		return false;
	}

	// outline inside
	font_painter->set_style(FONT_STYLE_NORMAL);
	font_painter->set_color(
		hover_over ? color_state->hot_text_color : color_state->idle_text_color);
	font_painter->set_xy(pos[0] + (pos[2] / 2.f), pos[1] + (pos[3] / 2.f));
	if(!font_painter->draw_text(text.c_str(), text.size()))
	{
		return false;
	}

	font_painter->end();

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
void mono_button_object::unfocus()
{
	hover_over = false;
}

bool option_menu_state::init(
	font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader)
{
	ASSERT(font_ != NULL);
	ASSERT(batcher_ != NULL);

	font_painter.init(batcher_, font_);

	for(const auto& [key, value] : get_keybinds())
	{
		buttons.emplace_back(value);
		buttons.back().button.init(&font_painter, &button_color_config);
		buttons.back().button.text = value.cvar_write();
	}

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

	// scrollbar input
	if(scroll_h > (box_ymax - box_ymin))
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
							font_painter.state.font->get_lineskip();
				// clamp
				scroll_y = std::max(0.f, std::min(scroll_h - (box_ymax - box_ymin), scroll_y));
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

	if(input_keybind_requested)
	{
		ASSERT(requested_keybind_index != -1);
		cvar_key_bind& keybind = buttons.at(requested_keybind_index).keybind;
		if(keybind.key_bind_count < cvar_key_bind::MAX_KEY_BINDS)
		{
			if(keybind.bind_sdl_event(e, &keybind.key_binds[keybind.key_bind_count]))
			{
				++keybind.key_bind_count;
				slogf("new bind: %s\n", keybind.cvar_write().c_str());
				input_keybind_requested = false;
		        mono_button_object& button = buttons.at(requested_keybind_index).button;
                button.text = keybind.cvar_write();
			    return OPTION_MENU_RESULT::EAT;
			}
		}
		else
		{
			input_keybind_requested = false;
		}
	}

	int index = 0;
	for(auto& button : buttons)
	{
        // too high
        if(box_ymin >= button.button.pos[1] + button.button.pos[3])
        {
            continue;
        }
        // too low
        if(box_ymax <= button.button.pos[1])
        {
            break;
        }
		switch(button.button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			input_keybind_requested = true;
			button.keybind.key_bind_count = 0;
			requested_keybind_index = index;
			return OPTION_MENU_RESULT::EAT;
		case BUTTON_RESULT::ERROR: return OPTION_MENU_RESULT::ERROR;
		}
		++index;
	}

    //backdrop
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

bool option_menu_state::draw_buffer()
{
	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;

	batcher->clear();

    // TODO: fullscreen makes the buttons too big, maybe shrink it?
	box_xmin = 60;
	box_xmax = static_cast<float>(cv_screen_width.data) - 60;
	box_ymin = 60;
	box_ymax = static_cast<float>(cv_screen_height.data) - 60;

    float padding = 10;
    float button_height = font_painter.state.font->get_lineskip() + 4;

    // set the buttons dimensions
    {
        float x = ((box_xmax-box_xmin)-padding) / 2 + padding;
        float y = 0;
        float width = (box_xmax-box_xmin) - scrollbar_thickness - padding - x;
        for(auto & entry : buttons)
        {
            entry.button.set_rect(box_xmin + x, box_ymin + y - scroll_y, width, button_height);
            y += button_height + padding;
        }
        // very important to do this before drawing the scrollbar.
        scroll_h = y - padding;
    }

    // draw the backdrop bbox
	{
        float xmin = box_xmin;
        float xmax = box_xmax;
        float ymin = box_ymin;
        float ymax = box_ymax;

        std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
        std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

        batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
        batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
        batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
        batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
        batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}

	if(scroll_h > (box_ymax - box_ymin))
	{
		// draw the scrollbar bbox
		{
			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin;
			float ymax = box_ymax;

			std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
		// draw the scrollbar thumb
		{
			float scrollbar_max_height = (box_ymax - box_ymin);

			float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / scroll_h);
			thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

			float scroll_ratio =
				(scrollbar_max_height - thumb_height) / (scroll_h - (box_ymax - box_ymin));
			float thumb_offset = scroll_y * scroll_ratio;

			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin + thumb_offset;
			float ymax = box_ymin + thumb_offset + thumb_height;

			std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
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
		font_painter.begin();
		float y = 0;
		for(auto& button : buttons)
		{
			float y_pos = y;
			y += button_height + padding;

			// too high
			if(box_ymin >= button.button.pos[1] + button.button.pos[3])
			{
				continue;
			}
			// too low
			if(box_ymax <= button.button.pos[1])
			{
				break;
			}

			// outline
			font_painter.set_style(FONT_STYLE_OUTLINE);
			font_painter.set_color(0, 0, 0, 255);
			font_painter.set_anchor(TEXT_ANCHOR::CENTER_LEFT);
			font_painter.set_xy(
				box_xmin + padding, box_ymin + y_pos + button_height / 2.f - scroll_y);
			if(!font_painter.draw_format("%s:", button.keybind.cvar_comment))
			{
				return false;
			}

			// outline inside
			font_painter.set_style(FONT_STYLE_NORMAL);
			font_painter.set_color(255, 255, 255, 255);
			font_painter.set_xy(
				box_xmin + padding, box_ymin + y_pos + button_height / 2.f - scroll_y);

			if(!font_painter.draw_format("%s:", button.keybind.cvar_comment))
			{
				return false;
			}
		}
		font_painter.end();
	}

	for(auto& button : buttons)
	{
        // too high
        if(box_ymin >= button.button.pos[1] + button.button.pos[3])
        {
            continue;
        }
        // too low
        if(box_ymax <= button.button.pos[1])
        {
            break;
        }
		if(!button.button.draw_buffer())
		{
			return false;
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
	return true;
}

bool option_menu_state::render()
{
    // clamp the scroll (when the screen resizes)
    scroll_y = std::max(0.f, std::min(scroll_h - (box_ymax - box_ymin), scroll_y));

	if(!draw_buffer())
	{
		return false;
	}

	mono_2d_batcher* batcher = font_painter.state.batcher;
	if(batcher->get_quad_count() != 0)
	{
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
		ctx.glBufferData(GL_ARRAY_BUFFER, batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
		ctx.glBufferSubData(
			GL_ARRAY_BUFFER, 0, batcher->get_current_vertex_size(), batcher->buffer);
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

		float x = box_xmin;
		float y = box_ymin;
		float w = box_xmax - box_xmin;
		float h = box_ymax - box_ymin;
		GLint scissor_x = static_cast<GLint>(x);
		GLint scissor_y = static_cast<GLint>(y);
		GLint scissor_w = static_cast<GLint>(w);
		GLint scissor_h = static_cast<GLint>(h);
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glBindVertexArray(gl_options_vao_id);
			ctx.glDrawArrays(GL_TRIANGLES, 0, batcher->get_current_vertex_count());
			ctx.glBindVertexArray(0);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
	}
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void option_menu_state::unfocus()
{
    y_scrollbar_held = false;
    scroll_drag_y = -1;
    scroll_thumb_click_offset = -1;

	for(auto& button : buttons)
	{
		button.button.unfocus();
	}
}


void option_menu_state::internal_scroll_y_to(float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / scroll_h);
    thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

    float scroll_ratio =
        (scrollbar_max_height - thumb_height) / (scroll_h - (box_ymax - box_ymin));
    scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;

    // clamp
	scroll_y = std::max(0.f, std::min(scroll_h - (box_ymax - box_ymin), scroll_y));
}

bool option_menu_state::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / scroll_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio = (scrollbar_max_height - thumb_height) / (scroll_h - (box_ymax - box_ymin));
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