#include "../global_pch.h"
#include "../global.h"

#include "pointsprite.h"

static const char* shader_point_sprite_vs = R"(#version 300 es
precision mediump float;

uniform sampler2D u_inst_table;
uniform mat4 u_proj;
uniform mat4 u_view;

in vec3 a_vert_pos;
in vec3 a_point_pos;
in vec4 a_point_color;
//in int a_point_inst_id;

out vec4 frag_color;

mat4 Get_Matrix(int offset)
{
    return (mat4(texelFetch(u_inst_table, ivec2(offset, 0), 0), 
		texelFetch(u_inst_table, ivec2(offset + 1, 0), 0), 
			texelFetch(u_inst_table, ivec2(offset + 2, 0), 0),
                texelFetch(u_inst_table, ivec2(offset + 3, 0), 0)));
}

void main()
{
    //TODO: this is just a placeholder, I have given up on pointsprite so I don't really care.
	mat4 model = Get_Matrix(0);
	//remove the rotation of the model from the point
	//I don't actually understand the math this is trial and error
	vec3 pos = a_point_pos + inverse(mat3(model)) * a_vert_pos;
	gl_Position = u_proj * u_view * model * vec4(pos, 1.f);
    frag_color = a_point_color;
}
)";

static const char* shader_point_sprite_fs = R"(#version 300 es
precision mediump float;
in vec4 frag_color;
out vec4 color;
void main()
{
    color = frag_color;
}
)";

bool shader_pointsprite_state::create()
{
	gl_program_id = gl_create_program(
		"shader_point_sprite_vs",
		shader_point_sprite_vs,
		"shader_point_sprite_fs",
		shader_point_sprite_fs);
	if(gl_program_id == 0)
	{
		return false;
	}

    const char* info = "shader_point_sprit";

	SET_GL_UNIFORM_ID(info, u_inst_table);
	SET_GL_UNIFORM_ID(info, u_proj);
	SET_GL_UNIFORM_ID(info, u_view);

	SET_GL_ATTRIBUTE_ID(info, a_vert_pos);
	SET_GL_ATTRIBUTE_ID(info, a_point_pos);
	SET_GL_ATTRIBUTE_ID(info, a_point_color);
	// SET_GL_ATTRIBUTE_ID(a_point_inst_id);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool shader_pointsprite_state::destroy()
{
	if(gl_program_id != 0)
	{
		ctx.glDeleteProgram(gl_program_id);
		gl_program_id = 0;
	}
	return GL_CHECK(__func__) == GL_NO_ERROR;
}
