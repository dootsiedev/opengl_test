#pragma once

#include "../opengles2/opengl_stuff.h"

struct shader_pointsprite_state
{
    GLuint gl_program_id = 0;

    struct
	{
		// GLint u_inst_color_palette;
        GLint u_inst_table = -1;
		GLint u_proj = -1;
		GLint u_view = -1;
	} gl_uniforms;

	struct
	{
		GLint a_vert_pos = -1;
		GLint a_point_pos = -1;
		GLint a_point_color = -1;
		//GLint a_point_inst_id = -1;
	} gl_attributes;

    bool create();
    bool destroy();
};
