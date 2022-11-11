#include "../global_pch.h"
#include "../global.h"

#include "mono.h"

static const char* shader_mono_vs = R"(#version 300 es
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

static const char* shader_mono_fs = R"(#version 300 es
precision mediump float;

uniform sampler2D u_tex;

in vec2 tex_coord;
in vec4 vert_color;

out vec4 color;

void main()
{
    // premultiplied in the shader
    // this is commented out because I do this outside the shader, or I don't use premultiplied blending.
    // vec4 texel = texture(u_tex, tex_coord).r * vec4(vert_color.rgb,1) * vert_color.a;

    color = texture(u_tex, tex_coord).r * vert_color;
}
)";

// this should be snprintf'd to set the value that the alpha needs.
// but if I needed this to be runtime modified, I could put the value into a uniform.
static const char* shader_mono_alpha_test_fs = R"(#version 300 es
precision mediump float;

uniform sampler2D u_tex;
uniform float u_alpha_test;

in vec2 tex_coord;
in vec4 vert_color;

out vec4 color;

void main()
{
    // this substitutes glAlphaFunc(GL_GEQUAL, x)
    if(texture(u_tex, tex_coord).r < u_alpha_test)
    {
        discard;
    }

    color = vert_color;
}
)";

bool shader_mono_state::create()
{
	info = "shader_mono";

	gl_program_id =
		gl_create_program("shader_mono_vs", shader_mono_vs, "shader_mono_fs", shader_mono_fs);
	if(gl_program_id == 0)
	{
		return false;
	}

	internal_find_locations();

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool shader_mono_state::create_alpha_test()
{
	info = "shader_mono_alpha_test";

	gl_program_id = gl_create_program(
		"shader_mono_vs", shader_mono_vs, "shader_mono_alpha_test_fs", shader_mono_alpha_test_fs);
	if(gl_program_id == 0)
	{
		return false;
	}

	internal_find_locations();

	// extra uniform for alpha testing
	SET_GL_UNIFORM_ID(info, u_alpha_test);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

void shader_mono_state::internal_find_locations()
{
	SET_GL_UNIFORM_ID(info, u_tex);
	SET_GL_UNIFORM_ID(info, u_mvp);

	SET_GL_ATTRIBUTE_ID(info, a_pos);
	SET_GL_ATTRIBUTE_ID(info, a_tex);
	SET_GL_ATTRIBUTE_ID(info, a_color);
}

bool shader_mono_state::destroy()
{
	if(gl_program_id != 0)
	{
		ctx.glDeleteProgram(gl_program_id);
		gl_program_id = 0;
	}
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

void gl_create_interleaved_mono_vertex_vao(shader_mono_state& mono_shader)
{
	if(mono_shader.gl_attributes.a_pos != -1)
	{
		ctx.glEnableVertexAttribArray(mono_shader.gl_attributes.a_pos);
		ctx.glVertexAttribPointer(
			mono_shader.gl_attributes.a_pos, // attribute
			3, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			sizeof(gl_mono_vertex), // stride
			(void*)offsetof(gl_mono_vertex, pos) // NOLINT
		);
	}
	if(mono_shader.gl_attributes.a_tex != -1)
	{
		ctx.glEnableVertexAttribArray(mono_shader.gl_attributes.a_tex);
		ctx.glVertexAttribPointer(
			mono_shader.gl_attributes.a_tex, // attribute
			2, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			sizeof(gl_mono_vertex), // stride
			(void*)offsetof(gl_mono_vertex, tex) // NOLINT
		);
	}
	if(mono_shader.gl_attributes.a_color != -1)
	{
		ctx.glEnableVertexAttribArray(mono_shader.gl_attributes.a_color);
		ctx.glVertexAttribPointer(
			mono_shader.gl_attributes.a_color, // attribute
			4, // size
			GL_UNSIGNED_BYTE, // type
			GL_TRUE, // normalized?
			sizeof(gl_mono_vertex), // stride
			(void*)offsetof(gl_mono_vertex, color) // NOLINT
		);
	}
}
