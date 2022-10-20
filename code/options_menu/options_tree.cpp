#include "../global_pch.h"
#include "../global.h"

#include "options_tree.h"

#include "../app.h"

// for the cvars...
#include "../demo.h"

// TODO: BIG PROBLEM if I modify a cvar (like fullscreen) outside the menu,
// the change will not appear in the menu even if you close and open it...
// I think I need a bool open() callback... or just refresh every second...

bool options_tree_state::init(
	font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader)
{
	ASSERT(font_ != NULL);
	ASSERT(batcher_ != NULL);

	font_painter.init(batcher_, font_);
	// font_painter.set_scale(2);

	done_text = "Done";
	done_button.init(&font_painter);

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

	if(GL_CHECK(__func__) != GL_NO_ERROR)
	{
		return false;
	}

	shared_menu_state.init(&font_painter, gl_options_interleave_vbo, gl_options_vao_id);

	// video

	{
		auto& back = menus.emplace_back();
		back.text = "Video";
		back.button.init(&font_painter);

		back.add_option(create_bool_option(&shared_menu_state, "fullscreen", &cv_fullscreen));
		back.add_option(create_bool_option(&shared_menu_state, "vsync", &cv_vsync));
		back.add_option(
			create_slider_option(&shared_menu_state, "scale font*", &cv_ui_scale, 1, 4));
		// TODO: add a new option that is a one_shot button
		// so that I can add in a "resize window", for people who want a specific resolution
		back.add_option(create_prompt_option(
			&shared_menu_state, "startup screen width*", &cv_startup_screen_width));
		back.add_option(create_prompt_option(
			&shared_menu_state, "startup screen height*", &cv_startup_screen_height));
		back.add_option(create_bool_option(
			&shared_menu_state, "font linear filtering*", &cv_font_linear_filtering));

		back.add_option(
			create_prompt_option(&shared_menu_state, "font path*", &cv_string_font, true));
		back.add_option(create_prompt_option(&shared_menu_state, "font size*", &cv_string_pt));
		back.add_option(
			create_prompt_option(&shared_menu_state, "font outline*", &cv_string_outline));
		back.add_option(create_bool_option(&shared_menu_state, "font mono*", &cv_string_mono));
		back.add_option(create_bool_option(
			&shared_menu_state, "font bitmap outline*", &cv_string_force_bitmap));

		back.menu_state.show_footer_text = true;
		back.menu_state.footer_text = "(*) restart required";

		if(!back.good())
		{
			return false;
		}

		if(!back.menu_state.init(&shared_menu_state))
		{
			return false;
		}
	}

	// controls
	{
		auto& back = menus.emplace_back();
		back.text = "Controls";
		back.button.init(&font_painter);

		// TODO: if you hover your mouse over, show the cvar string and the description in a
		// tooltip.

		back.add_option(create_bool_option(&shared_menu_state, "invert mouse", &cv_mouse_invert));
		back.add_option(
			create_slider_option(&shared_menu_state, "mouse speed", &cv_mouse_sensitivity, 0, 1));
		back.add_option(
			create_slider_option(&shared_menu_state, "camera speed", &cv_camera_speed, 0, 100));
		back.add_option(
			create_slider_option(&shared_menu_state, "scroll speed", &cv_scroll_speed, 0, 10));
		// TODO: would be smart to have a dummy entry that is just text which says "key binds"
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind forward", &cv_bind_move_forward));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind backwards", &cv_bind_move_backward));
		back.add_option(create_keybind_option(&shared_menu_state, "bind left", &cv_bind_move_left));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind right", &cv_bind_move_right));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind jump / lift", &cv_bind_move_jump));
		back.add_option(create_keybind_option(
			&shared_menu_state, "bind crouch / decend", &cv_bind_move_crouch));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind fullscreen", &cv_bind_fullscreen));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind console", &cv_bind_open_console));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind options", &cv_bind_open_options));
		back.add_option(create_keybind_option(
			&shared_menu_state, "bind reset window", &cv_bind_reset_window_size));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind toggle text", &cv_bind_toggle_text));
		back.add_option(
			create_keybind_option(&shared_menu_state, "bind soft reboot", &cv_bind_soft_reboot));

		if(!back.good())
		{
			return false;
		}

		if(!back.menu_state.init(&shared_menu_state))
		{
			return false;
		}
	}

	tree_resize_view();

	return true;
}

bool options_tree_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_options_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_options_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

OPTIONS_MENU_RESULT options_tree_state::tree_input(SDL_Event& e)
{
	if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
	{
		tree_resize_view();
	}

	for(auto it = menus.begin(); it != menus.end(); ++it)
	{
		switch(it->button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// NOLINTNEXTLINE(bugprone-narrowing-conversions)
			current_menu_index = std::distance(menus.begin(), it);
			return OPTIONS_MENU_RESULT::CLOSE;
		case BUTTON_RESULT::ERROR: return OPTIONS_MENU_RESULT::ERROR;
		}
	}

	switch(done_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER: return OPTIONS_MENU_RESULT::CLOSE;
	case BUTTON_RESULT::ERROR: return OPTIONS_MENU_RESULT::ERROR;
	}

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		// close counts as an eat.
		return OPTIONS_MENU_RESULT::CLOSE;
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
				return OPTIONS_MENU_RESULT::CONTINUE;
			}
		}
		break;
	}

	return OPTIONS_MENU_RESULT::CONTINUE;
}

bool options_tree_state::tree_update(double delta_sec)
{
	for(auto& entry : menus)
	{
		entry.button.update(delta_sec);
	}
	done_button.update(delta_sec);
	return true;
}

bool options_tree_state::tree_render()
{
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	for(auto& entry : menus)
	{
		if(entry.button.draw_requested())
		{
			tree_draw_buffer = true;
			break;
		}
	}

	tree_draw_buffer = tree_draw_buffer || done_button.draw_requested();

	if(tree_draw_buffer)
	{
		tree_draw_buffer = false;

		mono_2d_batcher* batcher = font_painter.state.batcher;

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
		for(auto& entry : menus)
		{
			if(!entry.button.draw_buffer(entry.text.c_str(), entry.text.size()))
			{
				return false;
			}
		}
		if(!done_button.draw_buffer(done_text.c_str(), done_text.size()))
		{
			return false;
		}

		if(batcher->get_quad_count() != 0)
		{
			// upload
			ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
			ctx.glBufferData(
				GL_ARRAY_BUFFER, batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
			ctx.glBufferSubData(
				GL_ARRAY_BUFFER, 0, batcher->get_current_vertex_size(), batcher->buffer);
			ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		gl_batch_vertex_count = batcher->get_current_vertex_count();
	}

	if(gl_batch_vertex_count != 0)
	{
		// draw
		ctx.glBindVertexArray(gl_options_vao_id);
		ctx.glDrawArrays(GL_TRIANGLES, 0, gl_batch_vertex_count);
		ctx.glBindVertexArray(0);
	}
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

bool options_tree_state::refresh()
{
	if(current_menu_index != -1)
	{
		return menus.at(current_menu_index).menu_state.refresh();
	}
	return true;
}

OPTIONS_MENU_RESULT options_tree_state::input(SDL_Event& e)
{
	if(current_menu_index == -1)
	{
		switch(tree_input(e))
		{
		case OPTIONS_MENU_RESULT::CONTINUE: return OPTIONS_MENU_RESULT::CONTINUE;
		case OPTIONS_MENU_RESULT::CLOSE: {
			SDL_Event fake_event;
			set_event_hidden(fake_event);
			if(tree_input(fake_event) == OPTIONS_MENU_RESULT::ERROR)
			{
				return OPTIONS_MENU_RESULT::ERROR;
			}
			// a stupid hack, if the selector opened a menu, it would modify current_menu_index
			if(current_menu_index == -1)
			{
				// eat
				set_event_unfocus(e);
				return OPTIONS_MENU_RESULT::CLOSE;
			}
			// if the screen resized, you need to fix the resize.
			set_event_resize(fake_event);
			if(menus.at(current_menu_index).menu_state.input(fake_event) ==
			   OPTIONS_MENU_RESULT::ERROR)
			{
				return OPTIONS_MENU_RESULT::ERROR;
			}
			if(!menus.at(current_menu_index).menu_state.refresh())
			{
				return OPTIONS_MENU_RESULT::ERROR;
			}
			// eat
			set_event_unfocus(e);
			return OPTIONS_MENU_RESULT::CONTINUE;
		}

		case OPTIONS_MENU_RESULT::ERROR: return OPTIONS_MENU_RESULT::ERROR;
		}
		ASSERT(false);
		serrf("%s: unknown switch", __func__);
		return OPTIONS_MENU_RESULT::ERROR;
	}
	switch(menus.at(current_menu_index).menu_state.input(e))
	{
	case OPTIONS_MENU_RESULT::CONTINUE: return OPTIONS_MENU_RESULT::CONTINUE;
	case OPTIONS_MENU_RESULT::CLOSE: {
		SDL_Event fake_event;
		set_event_hidden(fake_event);
		if(menus.at(current_menu_index).menu_state.input(fake_event) == OPTIONS_MENU_RESULT::ERROR)
		{
			return OPTIONS_MENU_RESULT::ERROR;
		}
		if(!menus.at(current_menu_index).menu_state.close())
		{
			return OPTIONS_MENU_RESULT::ERROR;
		}
		// if the screen resized, you need to fix the resize.
		set_event_resize(fake_event);
		if(tree_input(fake_event) == OPTIONS_MENU_RESULT::ERROR)
		{
			return OPTIONS_MENU_RESULT::ERROR;
		}
		current_menu_index = -1;
		// eat
		set_event_unfocus(e);
		return OPTIONS_MENU_RESULT::CONTINUE;
	}
	case OPTIONS_MENU_RESULT::ERROR: return OPTIONS_MENU_RESULT::ERROR;
	}
	ASSERT(false);
	serrf("%s: unknown switch", __func__);
	return OPTIONS_MENU_RESULT::ERROR;
}

bool options_tree_state::update(double delta_sec)
{
	if(current_menu_index == -1)
	{
		return tree_update(delta_sec);
	}
	return menus.at(current_menu_index).menu_state.update(delta_sec);
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_tree_state::render()
{
	if(current_menu_index == -1)
	{
		return tree_render();
	}
	return menus.at(current_menu_index).menu_state.render();
}

void options_tree_state::tree_resize_view()
{
	float font_padding = shared_menu_state.font_padding;
	float element_padding = shared_menu_state.element_padding;

	// for a 16px font I would want 200px
	float button_width = 200 * (font_painter.get_lineskip() / 16.f);
	float button_height = font_painter.get_lineskip() + font_padding;

	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	// + 1 for the Done button.
	float button_count = static_cast<float>(menus.size()) + 1.f;

	float button_area_height =
		button_height * button_count + element_padding * (button_count - 1.f);

	float x = std::floor((screen_width - button_width) / 2.f);
	float y = std::floor((screen_height - button_area_height) / 2.f);

	float cur_y = y;
	for(auto& entry : menus)
	{
		entry.button.set_rect(x, cur_y, button_width, button_height);
		cur_y += button_height + element_padding;
	}
	done_button.set_rect(x, cur_y, button_width, button_height);

	box_xmin = x - element_padding;
	box_xmax = x + button_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + button_area_height + element_padding;

	tree_draw_buffer = true;
}