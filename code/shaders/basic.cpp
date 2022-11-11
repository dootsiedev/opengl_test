#include "../global_pch.h"
#include "../global.h"

#include "basic.h"

static const char* shader_basic_vs = R"(#version 300 es
precision mediump float;

uniform mat4 u_mvp;

in vec3 a_pos;
in vec2 a_tex;
in vec4 a_color;

out vec2 tex_coord;
out vec4 vert_color;

void main()
{
	gl_Position = u_mvp * vec4(a_pos, 1.0);
    tex_coord = a_tex;
	vert_color = a_color;
}
)";

static const char* shader_basic_fs = R"(#version 300 es
precision mediump float;

uniform sampler2D u_tex;

in vec2 tex_coord;
in vec4 vert_color;

out vec4 color;

void main()
{
    //premultiplied (only used for linear filtering)
    //vec4 texel = texture(u_tex, tex_coord) * vec4(vert_color.rgb,1) * vert_color.a;

    vec4 texel = texture(u_tex, tex_coord) * vert_color;

    color = texel;
}
)";

bool shader_basic_state::create()
{
	gl_program_id =
		gl_create_program("shader_basic_vs", shader_basic_vs, "shader_basic_fs", shader_basic_fs);
	if(gl_program_id == 0)
	{
		return false;
	}
    const char* info = "shader_basic";
    
	SET_GL_UNIFORM_ID(info, u_tex);
	SET_GL_UNIFORM_ID(info, u_mvp);

	SET_GL_ATTRIBUTE_ID(info, a_pos);
	SET_GL_ATTRIBUTE_ID(info, a_tex);
	SET_GL_ATTRIBUTE_ID(info, a_color);
    
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool shader_basic_state::destroy()
{
	if(gl_program_id != 0)
	{
		ctx.glDeleteProgram(gl_program_id);
		gl_program_id = 0;
	}
	return GL_CHECK(__func__) == GL_NO_ERROR;
}