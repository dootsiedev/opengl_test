#pragma once

// opengles3

#include "gl3platform.h"
#include "gl3.h"
#include "gl2ext.h"

#define SAFE_GL_DELETE_VBO(x)             \
	do                                    \
	{                                     \
		if((x) != 0)                      \
		{                                 \
			ctx.glDeleteBuffers(1, &(x)); \
			(x) = 0;                      \
		}                                 \
	} while(0)
#define SAFE_GL_DELETE_VAO(x)                  \
	do                                         \
	{                                          \
		if((x) != 0)                           \
		{                                      \
			ctx.glDeleteVertexArrays(1, &(x)); \
			(x) = 0;                           \
		}                                      \
	} while(0)
#define SAFE_GL_DELETE_TEXTURE(x)          \
	do                                     \
	{                                      \
		if((x) != 0)                       \
		{                                  \
			ctx.glDeleteTextures(1, &(x)); \
			(x) = 0;                       \
		}                                  \
	} while(0)

// returns true if no errors occurred
GLenum implement_GL_CHECK(const char* msg, const char* file, int line);
#define GL_CHECK(msg) implement_GL_CHECK(msg, __FILE__, __LINE__)

// on release builds, this will be a NOP
GLenum implement_GL_RUNTIME(const char* msg, const char* file, int line);
#define GL_RUNTIME(msg) implement_GL_RUNTIME(msg, __FILE__, __LINE__)

struct GLES2_Context
{
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define SDL_PROC(ret, func, params) ret(GL_APIENTRY* func) params;
#define NULL_PROC SDL_PROC
#include "SDL_gles2funcs.h.txt"
#undef NULL_PROC
#undef SDL_PROC
};

bool LoadGLContext(GLES2_Context* data);

extern GLES2_Context ctx;

GLuint gl_create_program(
	const char* vert_info,
	const GLchar* vert_shader,
	const char* frag_info,
	const GLchar* frag_shader);

// gl_create_program but you can use an array of strings.
GLuint gl_create_program2(
	const char* vert_info,
	int vert_count,
	const GLchar* const* vert_shader,
	const char* frag_info,
	int frag_count,
	const GLchar* const* frag_shader);