#include "global.h"

#include "options.h"
#include "app.h"

bool option_menu_state::init(
    font_style_interface* font_,
    mono_2d_batcher* batcher_,
    shader_mono_state& mono_shader)
{
	ASSERT(font_ != NULL);
	ASSERT(batcher_ != NULL);

	options_font = font_;
	options_batcher = batcher_;

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

    draw_buffer();

	return GL_CHECK(__func__) == GL_NO_ERROR ;
}

bool option_menu_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_options_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_options_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR ;
}

OPTION_MENU_RESULT option_menu_state::input(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: draw_buffer(); break;
		}
	}
    return OPTION_MENU_RESULT::CONTINUE;
}

void option_menu_state::draw_buffer()
{
    options_batcher->set_cursor(0);

	auto white_uv = options_font->get_font_atlas()->white_uv;

    float xmin = 60;
    float xmax = static_cast<float>(cv_screen_width.data) - 60;
    float ymin = 60;
    float ymax = static_cast<float>(cv_screen_height.data) - 60;

    std::array<uint8_t,4> bbox_color{0,0,0,255};
    std::array<uint8_t,4> fill_color = RGBA8_PREMULT(50,50,50,200);

    options_batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
    options_batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
    options_batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
    options_batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
    options_batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
    
    batcher_vertex_count = options_batcher->get_current_vertex_count();
    if(batcher_vertex_count != 0)
    {
        ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_options_interleave_vbo);
        ctx.glBufferData(
            GL_ARRAY_BUFFER, options_batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
        ctx.glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            options_batcher->get_current_vertex_size(),
            options_batcher->buffer);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

bool option_menu_state::update()
{
    return true;
}

bool option_menu_state::render()
{
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

}