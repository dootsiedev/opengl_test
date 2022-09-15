#pragma once

#include "../global.h"

#include <array>
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

// this is a vao that works with interleaved gl_mono_vertex
// bind the VBO and VAO
void gl_create_interleaved_mono_vertex_vao(shader_mono_state& mono_shader);

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

struct mono_2d_batcher
{
	enum
	{
		QUAD_VERTS = 6
	};

	gl_mono_vertex* buffer = NULL;
	size_t size = 0;
	size_t cursor = 0;

	void init(gl_mono_vertex* buffer_, size_t size_)
	{
		ASSERT(buffer_);
		buffer = buffer_;
		size = size_;
	}

	GLsizei get_current_vertex_count() const
	{
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		return cursor * QUAD_VERTS;
	}
	GLsizeiptr get_current_vertex_size() const
	{
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		return cursor * QUAD_VERTS * sizeof(gl_mono_vertex);
	}
	GLsizeiptr get_total_vertex_size() const
	{
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		return size * QUAD_VERTS * sizeof(gl_mono_vertex);
	}
	size_t get_quad_count() const
	{
		ASSERT(buffer != NULL);
		return cursor;
	}
    void clear()
    {
		ASSERT(buffer != NULL);
        cursor = 0;
    }
	// use the return from get_quad_count
	NDSERR bool set_cursor(size_t pos)
	{
		ASSERT(buffer != NULL);
		ASSERT(pos < size);
		if(pos >= size)
        {
            serrf("%s out of bounds: %zu (size: %zu)\n", __func__, pos, size);
            return false;
        }
		cursor = pos;
        return true;
	}

	// [0]=minx,[1]=miny,[2]=maxx,[3]=maxy
	bool draw_rect(std::array<float, 4> pos, std::array<float, 4> uv, std::array<uint8_t, 4> color)
	{
		ASSERT(buffer != NULL);
		if(cursor >= size)
		{
			return false;
		}
		return draw_rect_at(cursor++, pos, uv, color);
	}

	int placeholder_rect()
	{
		ASSERT(buffer != NULL);
		if(cursor >= size)
		{
			return -1;
		}
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		return cursor++;
	}

	// index is the quad index.
	// [0]=minx,[1]=miny,[2]=maxx,[3]=maxy
	bool draw_rect_at(
		size_t index,
		std::array<float, 4> pos,
		std::array<float, 4> uv,
		std::array<uint8_t, 4> color)
	{
		ASSERT(buffer != NULL);
		if(index >= size)
		{
			return false;
		}
		gl_mono_vertex* cur = buffer + index * QUAD_VERTS;
		*cur++ = {pos[0], pos[1], 0.f, uv[0], uv[1], color[0], color[1], color[2], color[3]};
		*cur++ = {pos[2], pos[3], 0.f, uv[2], uv[3], color[0], color[1], color[2], color[3]};
		*cur++ = {pos[2], pos[1], 0.f, uv[2], uv[1], color[0], color[1], color[2], color[3]};
		*cur++ = {pos[0], pos[1], 0.f, uv[0], uv[1], color[0], color[1], color[2], color[3]};
		*cur++ = {pos[0], pos[3], 0.f, uv[0], uv[3], color[0], color[1], color[2], color[3]};
		*cur++ = {pos[2], pos[3], 0.f, uv[2], uv[3], color[0], color[1], color[2], color[3]};
		return true;
	}
};
