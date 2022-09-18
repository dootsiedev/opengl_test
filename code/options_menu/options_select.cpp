#include "../global.h"

#include "options_select.h"

#include "../app.h"

// TODO(dootsie): make the escape button close the menu, 
// and make a popup that asks if you want to keep the changes?
// TODO(dootsie): each keybind should have it's own "revert to default".

NDSERR bool options_select_state::init(font_sprite_painter *font_painter_, GLuint vbo, GLuint vao)
{
	ASSERT(font_painter_ != NULL);
    
    font_painter = font_painter_;

    resize_view();

    return true;
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

	#if 0
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
				return OPTIONS_KEYBINDS_RESULT::EAT;
			case BUTTON_RESULT::ERROR: return OPTIONS_KEYBINDS_RESULT::ERROR;
			}
		}
	

	if( //! input_eaten &&
		e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		unfocus();
        return OPTIONS_SELECT_RESULT::CLOSE;
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
				return OPTIONS_SELECT_RESULT::EAT;
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
				return OPTIONS_SELECT_RESULT::EAT;
			}

			unfocus();
		}
		break;
	}
#endif
	return OPTIONS_SELECT_RESULT::CONTINUE;
}

bool options_select_state::update(double delta_sec)
{
    #if 0
	for(auto& button : buttons)
	{
		if(!button.button.update(delta_sec))
		{
			return false;
		}
	}
    #endif
	return true;
}

bool options_select_state::render()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	batcher->clear();
#if 0

	if(!draw_base())
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
	}
    #endif
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void options_select_state::resize_view()
{
	box_xmin = 60;
	box_xmax = static_cast<float>(cv_screen_width.data) - 60;
	box_ymin = 60;
	box_ymax = static_cast<float>(cv_screen_height.data) - 60;
}

void options_select_state::unfocus()
{
    #if 0
	// all this does is make the buttons not hovered
	// since if you close the menu while hovering a button,
	// when you make the menu re-appear, and if you don't move
	// the mouse, the button will stay "hot".
	for(auto& button : buttons)
	{
		button.button.unfocus();
	}
    #endif
}