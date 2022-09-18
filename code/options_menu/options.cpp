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

    return keybinds.init(&font_painter, gl_options_interleave_vbo, gl_options_vao_id);
}

bool options_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_options_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_options_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

OPTIONS_RESULT options_state::input(SDL_Event& e)
{
    switch(keybinds.input(e))
    {
		case OPTIONS_KEYBINDS_RESULT::CONTINUE: return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_KEYBINDS_RESULT::EAT: return OPTIONS_RESULT::EAT;
		case OPTIONS_KEYBINDS_RESULT::CLOSE: return OPTIONS_RESULT::CLOSE;
		case OPTIONS_KEYBINDS_RESULT::ERROR: return OPTIONS_RESULT::ERROR;
    }
    return OPTIONS_RESULT::CONTINUE;
}

bool options_state::update(double delta_sec)
{
    return keybinds.update(delta_sec);
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_state::render()
{
    return keybinds.render();
}

void options_state::resize_view()
{
    keybinds.resize_view();
}

// call this when you need to unfocus, like for example if you press escape or something.
void options_state::unfocus()
{
    keybinds.unfocus();
}