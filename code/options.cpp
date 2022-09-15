#include "global.h"

#include "options.h"
#include "app.h"

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

	float xmin = pos[0];
	float xmax = pos[0] + pos[2];
	float ymin = pos[1];
	float ymax = pos[1] + pos[3];

	// normalize the colors 0-1
	float hot_fill_f[4] = {
		static_cast<float>(color_state->hot_fill_color[0]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[1]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[2]) / 255.f,
		static_cast<float>(color_state->hot_fill_color[3]) / 255.f};
	float idle_fill_f[4] = {
		static_cast<float>(color_state->idle_fill_color[0]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[1]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[2]) / 255.f,
		static_cast<float>(color_state->idle_fill_color[3]) / 255.f};

	// blend the colors.
	std::array<uint8_t, 4> fill_color = {
		static_cast<uint8_t>((hot_fill_f[0] * fade + idle_fill_f[0] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill_f[1] * fade + idle_fill_f[1] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill_f[2] * fade + idle_fill_f[2] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill_f[3] * fade + idle_fill_f[3] * (1.f - fade)) * 255.f),
	};

	// fill
	batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);

	// bbox
	batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, color_state->bbox_color);
	batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, color_state->bbox_color);
	batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, color_state->bbox_color);
	batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, color_state->bbox_color);

	// font
	font_painter->begin();
	font_painter->set_color(
		hover_over ? color_state->hot_text_color : color_state->idle_text_color);
	font_painter->set_xy(xmin + (xmax - xmin) / 2.f, ymin + (ymax - ymin) / 2.f);
	font_painter->set_anchor(TEXT_ANCHOR::CENTER_PERFECT);
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

	float y = 0;
	float width = 400;
	float height = font_->get_lineskip() + 4;
	float padding = 10;
	for(const auto& [key, value] : get_keybinds())
	{
		buttons.emplace_back(value);
		buttons.back().button.init(&font_painter, &button_color_config);
		buttons.back().button.text = key;
		buttons.back().button.text += ": ";
		buttons.back().button.text += value.cvar_write();
		buttons.back().button.set_rect(0, y, width, height);
		y += height + padding;
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
                // I need this but I don't understand why I dont...
                /*mono_2d_batcher* batcher = font_painter.state.batcher;
                // draw just this button into the batcher.
                batcher->clear();
                if(!button.draw_buffer())
				{
					return OPTION_MENU_RESULT::ERROR;
				}
				ASSERT(batcher->get_quad_count() != 0);
				ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
				ctx.glBufferSubData(
					GL_ARRAY_BUFFER,
					button.batcher_vertex_start,
					batcher->get_current_vertex_size(),
					batcher->buffer);
				ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
				if(GL_RUNTIME(__func__) != GL_NO_ERROR)
                {
                    return OPTION_MENU_RESULT::ERROR;
                }*/
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

	return OPTION_MENU_RESULT::CONTINUE;
}

bool option_menu_state::draw_buffer()
{
	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;

	batcher->clear();

	float xmin = 60;
	float xmax = static_cast<float>(cv_screen_width.data) - 60;
	float ymin = 60;
	float ymax = static_cast<float>(cv_screen_height.data) - 60;

	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
	std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

	batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
	batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
	batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
	batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
	batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);

	for(auto& button : buttons)
	{
		if(!button.button.draw_buffer())
		{
			return false;
		}
	}

	batcher_vertex_count = batcher->get_current_vertex_count();
	
	return true;
}

bool option_menu_state::update(double delta_sec)
{
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
	for(auto& button : buttons)
	{
		if(!button.button.update(delta_sec))
		{
			return false;
		}
	}
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

bool option_menu_state::render()
{
	if(!draw_buffer())
	{
		return false;
	}

	if(batcher_vertex_count != 0)
	{
		mono_2d_batcher* batcher = font_painter.state.batcher;
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
		ctx.glBufferData(GL_ARRAY_BUFFER, batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
		ctx.glBufferSubData(
			GL_ARRAY_BUFFER, 0, batcher->get_current_vertex_size(), batcher->buffer);
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if(batcher_vertex_count != 0)
	{
		ctx.glBindVertexArray(gl_options_vao_id);
		ctx.glDrawArrays(GL_TRIANGLES, 0, batcher_vertex_count);
		ctx.glBindVertexArray(0);
	}
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void option_menu_state::unfocus()
{
	for(auto& button : buttons)
	{
		button.button.unfocus();
	}
}