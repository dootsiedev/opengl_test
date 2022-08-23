#pragma once

#include "../opengles2/opengl_stuff.h"

struct shader_mono_state
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

struct gl_mono_vertex
{
	gl_mono_vertex() = default;
	// for emplace_back
	gl_mono_vertex(
		GLfloat posx,
		GLfloat posy,
		GLfloat posz,
		GLfloat texx,
		GLfloat texy,
		GLubyte r,
		GLubyte g,
		GLubyte b,
		GLubyte a)
	: pos{posx, posy, posz}
	, tex{texx, texy}
	, color{r, g, b, a}
	{
	}
	GLfloat pos[3];
	GLfloat tex[2];
	GLubyte color[4];
};

// this is a vao that works with interleaved gl_mono_vertex
// bind the VBO and VAO
void gl_mono_vertex_vao(shader_mono_state& mono_shader);