#include "../global.h"

#include "options_mouse.h"
#include "../app.h"

// for the cvars...
#include "../demo.h"

bool options_controls_state::init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
{
	ASSERT(font_painter_ != NULL);

	font_painter = font_painter_;
	gl_options_interleave_vbo = vbo;
	gl_options_vao_id = vao;

	shared_state.init(font_painter);

	option_entries.emplace_back(
		create_bool_option(&shared_state, "invert mouse", &cv_mouse_invert));
	option_entries.emplace_back(
		create_slider_option(&shared_state, "mouse speed", &cv_mouse_sensitivity, 0, 1, true));
	option_entries.emplace_back(
		create_keybind_option(&shared_state, "forward", &cv_bind_move_forward));

    // check for errors.
	for(auto& entry : option_entries)
	{
		if(!entry)
		{
			return false;
		}
	}

	// footer buttons
	revert_button.init(font_painter);
	revert_text = "revert";
	revert_button.set_disabled(true);

	ok_button.init(font_painter);
	ok_text = "ok";

	defaults_button.init(font_painter);
	defaults_text = "set defaults";

	// scrollbar
	scroll_state.init(font_painter);
	scroll_state.scrollbar_padding = element_padding;

	resize_view();

	return true;
}

OPTIONS_CONTROLS_RESULT options_controls_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: resize_view(); break;
		}
	}

	if(shared_state.focus_element != NULL)
	{
		switch(shared_state.focus_element->input(e))
		{
		case FOCUS_ELEMENT_RESULT::CONTINUE: break;
		case FOCUS_ELEMENT_RESULT::CLOSE: shared_state.set_focus(NULL); break;
		case FOCUS_ELEMENT_RESULT::MODIFIED:
			shared_state.set_focus(NULL);
			revert_button.set_disabled(false);
			break;
		case FOCUS_ELEMENT_RESULT::ERROR: return OPTIONS_CONTROLS_RESULT::ERROR;
		}
		// eat
		set_event_leave(e);
	}

    // scroll

	scroll_state.input(e);

	float scroll_xmin = scroll_state.box_xmin;
	float scroll_xmax = scroll_state.box_xmax;
	float scroll_ymin = scroll_state.box_ymin;
	float scroll_ymax = scroll_state.box_ymax;

    bool clip_scrollbox = false;

	// filter out mouse events that are clipped out of the scroll_view
    // TODO: this should really be handled in a way to reduce copy-paste
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		if(!(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
			 scroll_xmin <= mouse_x))
		{
			// un hover all the elements.
            SDL_Event e2;
			set_event_leave(e2);
            for(auto& entry : option_entries)
	        {
                if(entry->input(e2) == OPTION_ELEMENT_RESULT::ERROR)
                {
                    return OPTIONS_CONTROLS_RESULT::ERROR;
                }
            }
            // skip the buttons motion event.
            clip_scrollbox = true;
		}
	}
	break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			if(!(scroll_ymax >= mouse_y && scroll_ymin <= mouse_y && scroll_xmax >= mouse_x &&
				 scroll_xmin <= mouse_x))
			{
				// skip the buttons.
				clip_scrollbox = true;
			}
		}
		break;
	}
    if(!clip_scrollbox)
    {
        for(auto& entry : option_entries)
        {
            switch(entry->input(e))
            {
            case OPTION_ELEMENT_RESULT::CONTINUE: break;
            case OPTION_ELEMENT_RESULT::MODIFIED: revert_button.set_disabled(false); break;
            case OPTION_ELEMENT_RESULT::ERROR: return OPTIONS_CONTROLS_RESULT::ERROR;
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
			if(!undo_history())
			{
				return OPTIONS_CONTROLS_RESULT::ERROR;
			}
			return OPTIONS_CONTROLS_RESULT::CONTINUE;

		case BUTTON_RESULT::ERROR: return OPTIONS_CONTROLS_RESULT::ERROR;
		}

		switch(ok_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("ok click\n");
			if(!close())
			{
				return OPTIONS_CONTROLS_RESULT::ERROR;
			}
			// close acts like an eat
			return OPTIONS_CONTROLS_RESULT::CLOSE;
		case BUTTON_RESULT::ERROR: return OPTIONS_CONTROLS_RESULT::ERROR;
		}

		switch(defaults_button.input(e))
		{
		case BUTTON_RESULT::CONTINUE: break;
		case BUTTON_RESULT::TRIGGER:
			// trigger will eat
			// slog("reset defaults click\n");
			if(!set_defaults())
			{
				return OPTIONS_CONTROLS_RESULT::ERROR;
			}
			return OPTIONS_CONTROLS_RESULT::CONTINUE;
		case BUTTON_RESULT::ERROR: return OPTIONS_CONTROLS_RESULT::ERROR;
		}
	}

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		// close counts as an eat.
        if(!close())
        {
				return OPTIONS_CONTROLS_RESULT::ERROR;
        }
		return OPTIONS_CONTROLS_RESULT::CLOSE;
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
				return OPTIONS_CONTROLS_RESULT::CONTINUE;
			}
		}
		break;
	}

	return OPTIONS_CONTROLS_RESULT::CONTINUE;
}

bool options_controls_state::update(double delta_sec)
{
    if(shared_state.focus_element != NULL)
	{
		if(!shared_state.focus_element->update(delta_sec))
		{
            return false;
		}
	}
	// invert_button.update(delta_sec);
	for(auto& entry : option_entries)
	{
		if(!entry->update(delta_sec))
		{
			return false;
		}
	}

	// footer
	revert_button.update(delta_sec);
	ok_button.update(delta_sec);
	defaults_button.update(delta_sec);

	return true;
}

bool options_controls_state::draw_menu()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};
    
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

    // footer buttons
    {
        if(!ok_button.draw_buffer(ok_text.c_str(), ok_text.size()))
        {
            return false;
        }
        if(!revert_button.draw_buffer(revert_text.c_str(), revert_text.size()))
        {
            return false;
        }
        if(!defaults_button.draw_buffer(defaults_text.c_str(), defaults_text.size()))
        {
            return false;
        }
    }
    return true;
}
    
bool options_controls_state::draw_scroll()
{
    // for a 16px font I would want 400px
    float menu_width = scroll_state.box_inner_xmax - scroll_state.box_xmin;
    float cur_y = scroll_state.box_ymin - scroll_state.scroll_y;

	float scroll_ymin = scroll_state.box_ymin;
	float scroll_ymax = scroll_state.box_ymax;

    for(auto& entry : option_entries)
	{
		// too high
		if(scroll_ymin >= cur_y + entry->get_height())
		{
		}
		else
		{
			// too low
			if(scroll_ymax <= cur_y)
			{
				break;
			}
			if(!entry->draw_buffer(scroll_state.box_xmin, cur_y, menu_width))
			{
				return false;
			}
		}
		cur_y += entry->get_height() + element_padding;
	}

	scroll_state.draw_buffer();

    return true;
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_controls_state::render()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;

    if(shared_state.focus_element != NULL)
	{
		if(shared_state.focus_element->draw_requested())
		{
            update_buffer = true;
		}
	}

	for(auto& entry : option_entries)
	{
		if(entry->draw_requested())
		{
			update_buffer = true;
			break;
		}
	}
	update_buffer = update_buffer || ok_button.draw_requested();
	update_buffer = update_buffer || revert_button.draw_requested();
	update_buffer = update_buffer || defaults_button.draw_requested();

	update_buffer = update_buffer || scroll_state.draw_requested();

    // upload the data to the GPU
	if(update_buffer)
	{

		batcher->clear();

        if(!draw_menu())
        {
            return false;
        }

        // I need to split the batch into 2 draw calls for clipping the scroll.
        menu_batch_vertex_count = batcher->get_current_vertex_count();

        if(!draw_scroll())
        {
            return false;
        }

        scroll_batch_vertex_count = batcher->get_current_vertex_count();

        if(shared_state.focus_element != NULL)
        {
            if(!shared_state.focus_element->draw_buffer())
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
        }

        update_buffer = false;
	}

    //
    // actual rendering
    //

    // bind the vao which is used for all the batches here
    ctx.glBindVertexArray(gl_options_vao_id);

    if(menu_batch_vertex_count != 0)
    {
        // the draw_menu() call
        ctx.glDrawArrays(GL_TRIANGLES, 0, menu_batch_vertex_count);
    }

    GLint vertex_offset = menu_batch_vertex_count;
    GLsizei vertex_count = scroll_batch_vertex_count - menu_batch_vertex_count;

    if(vertex_count != 0)
    {
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
                vertex_offset,
                vertex_count);
            ctx.glDisable(GL_SCISSOR_TEST);
        }
    }

    if(shared_state.focus_element != NULL)
    {
        // this requires the VAO.
        if(!shared_state.focus_element->render())
        {
            return false;
        }
    }

	ctx.glBindVertexArray(0);

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void options_controls_state::resize_view()
{
	float button_height = font_painter->get_lineskip() + font_padding;
	float footer_height = button_height;
    float window_edge_padding = 60;


	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	// for a 16px font I would want 400px
	float menu_width = std::min(400 * (font_painter->get_lineskip() / 16.f), screen_width - window_edge_padding * 2);

	float button_area_height = 0;
	for(auto& entry : option_entries)
	{
		button_area_height += entry->get_height() + element_padding;
	}

    scroll_state.content_h = button_area_height - element_padding;
	// the footer has element_padding, but button_area_height needs to be trimmed by element_padding
	// so it cancels itself out.
	float menu_height = std::min(button_area_height + footer_height, screen_height - window_edge_padding * 2);
	float x = std::floor((screen_width - menu_width) / 2.f);
	float y = std::floor((screen_height - menu_height) / 2.f);

	// footer buttons
	{
		// for a 16px font I would want 60px
		float button_width = 60 * (font_painter->get_lineskip() / 16.f);

		float x_cursor = x + menu_width;
		x_cursor -= button_width;
		ok_button.set_rect(
			x_cursor,
			y + menu_height - footer_height,
			button_width,
			button_height);
		x_cursor -= button_width + element_padding;
		revert_button.set_rect(
			x_cursor,
			y + menu_height - footer_height,
			button_width,
			button_height);
		// note I double the width here
		x_cursor -= (button_width * 2) + element_padding;
		defaults_button.set_rect(
			x_cursor,
			y + menu_height - footer_height,
			button_width * 2,
			button_height);
	}

	box_xmin = x - element_padding;
	box_xmax = x + menu_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + menu_height + element_padding;

    scroll_state.resize_view(
		box_xmin + element_padding,
		box_xmax - element_padding,
		box_ymin + element_padding,
		box_ymax - (footer_height + element_padding) - element_padding);

    if(shared_state.focus_element != NULL)
    {
        shared_state.focus_element->resize_view();
    }

    update_buffer = true;
}

bool options_controls_state::undo_history()
{
	for(auto& entry : option_entries)
	{
		if(!entry->undo_changes())
		{
			return false;
		}
	}
	revert_button.set_disabled(true);

	// undo_changes() should clear it.
	// clear_history();

	return true;
}

bool options_controls_state::clear_history()
{
	for(auto& entry : option_entries)
	{
		if(!entry->clear_history())
		{
			return false;
		}
	}

	revert_button.set_disabled(true);
	return true;
}

bool options_controls_state::set_defaults()
{
	for(auto& entry : option_entries)
	{
		if(!entry->set_default())
		{
			return false;
		}
	}

	// allow revert
	revert_button.set_disabled(false);

	return true;
}

bool options_controls_state::close()
{
	scroll_state.scroll_to_top();

    // TODO(dootsie): this should probably be done outside...
    if(!shared_state.set_focus(NULL))
    {
        return false;
    }
    
    for(auto& entry : option_entries)
	{
		if(!entry->close())
		{
			return false;
		}
	}
	return clear_history();
}