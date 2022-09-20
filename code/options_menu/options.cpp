#include "../global.h"
#include "options.h"

bool options_state::init(
	font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader)
{
	ASSERT(font_ != NULL);
	ASSERT(batcher_ != NULL);

	font_painter.init(batcher_, font_);

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

	select.init(&font_painter, gl_options_interleave_vbo, gl_options_vao_id);
	keybinds.init(&font_painter, gl_options_interleave_vbo, gl_options_vao_id);

	return true;
}

bool options_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_options_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_options_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

OPTIONS_RESULT options_state::input(SDL_Event& e)
{
	switch(current_state)
	{
	case MENU_FACTORY::MENU_SELECT:
		switch(select.input(e))
		{
		case OPTIONS_SELECT_RESULT::CONTINUE: return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_SELECT_RESULT::CLOSE: return OPTIONS_RESULT::CLOSE;
		case OPTIONS_SELECT_RESULT::ERROR: return OPTIONS_RESULT::ERROR;
		case OPTIONS_SELECT_RESULT::OPEN_CONTROLS:
			current_state = MENU_FACTORY::KEYBINDS;

            // fix screen resize...
            SDL_Event e2;
            set_event_resize(e2);
            keybinds.input(e2);
			return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_SELECT_RESULT::OPEN_VIDEO: return OPTIONS_RESULT::CLOSE;
		}
		break;
	case MENU_FACTORY::KEYBINDS:
		switch(keybinds.input(e))
		{
		case OPTIONS_KEYBINDS_RESULT::CONTINUE: return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_KEYBINDS_RESULT::CLOSE:
            // fix screen resize...
            SDL_Event e2;
            set_event_resize(e2);
            select.input(e2);
			current_state = MENU_FACTORY::MENU_SELECT;
			return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_KEYBINDS_RESULT::ERROR: return OPTIONS_RESULT::ERROR;
		}
		break;
	}
	ASSERT(false);
	serrf("%s: unknown switch", __func__);
	return OPTIONS_RESULT::ERROR;
}

bool options_state::update(double delta_sec)
{
	switch(current_state)
	{
	case MENU_FACTORY::MENU_SELECT: return select.update(delta_sec);
	case MENU_FACTORY::KEYBINDS: return keybinds.update(delta_sec);
	}
	ASSERT(false);
	serrf("%s: unknown switch", __func__);
	return false;
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_state::render()
{
	switch(current_state)
	{
	case MENU_FACTORY::MENU_SELECT: return select.render();
	case MENU_FACTORY::KEYBINDS: return keybinds.render();
	}
	ASSERT(false);
	serrf("%s: unknown switch", __func__);
	return false;
}

void options_state::resize_view()
{
	switch(current_state)
	{
	case MENU_FACTORY::MENU_SELECT: select.resize_view(); return;
	case MENU_FACTORY::KEYBINDS: keybinds.resize_view();
	}
	ASSERT(false);
	slogf("%s: unknown switch", __func__);
}