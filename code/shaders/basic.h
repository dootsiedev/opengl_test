#pragma once

#include "../opengles2/opengl_stuff.h"

struct shader_basic_state
{
    GLuint gl_program_id = 0;

    struct
	{
        GLint u_tex = -1;
		GLint u_mvp = -1;
	} gl_uniforms;

	struct
	{
		GLint a_pos = -1;
		GLint a_tex = -1;
		GLint a_color = -1;
	} gl_attributes;

    bool create();
    bool destroy();
};

