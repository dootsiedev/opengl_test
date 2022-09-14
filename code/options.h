#pragma once


#include "opengles2/opengl_stuff.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"

#include <SDL2/SDL.h>

enum class OPTION_MENU_RESULT
{
	EAT,
	CONTINUE,
	ERROR
};

struct option_menu_state
{
    font_style_interface* options_font = NULL;
	mono_2d_batcher* options_batcher = NULL;
	GLsizei batcher_vertex_count = 0;

	// the buffer that contains the menu rects and text
	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

    NDSERR bool init(
		font_style_interface* font_,
		mono_2d_batcher* batcher_,
		shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTION_MENU_RESULT input(SDL_Event& e);

	void draw_buffer();

	NDSERR bool update();

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	// call this when you need to unfocus, like for example if you press escape or something.
	void unfocus();
};