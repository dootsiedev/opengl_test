#include "../global.h"
#include "opengl_stuff.h"

// for cv_debug_opengl
#include "../app.h"
#include "../debug_tools.h"

#include <SDL2/SDL.h>

GLES2_Context ctx;

REGISTER_CVAR_INT(cv_has_EXT_disjoint_timer_query, 0, "0 = not found, 1 = found", CVAR_T::READONLY);
REGISTER_CVAR_INT(cv_has_GL_KHR_debug, 0, "0 = not found, 1 = found", CVAR_T::READONLY);

GLenum implement_GL_RUNTIME(const char* msg, const char* file, int line)
{
	if(cv_debug_opengl.data == 0)
	{
		return GL_NO_ERROR;
	}
	return implement_GL_CHECK(msg, file, line);
}

// not as useful as debug callbacks, but better than a number.
static const char* gl_err_string(GLenum glError)
{
#define GLES2_ERROR(x) \
	case(x): return (#x);
	switch(glError)
	{
		GLES2_ERROR(GL_NO_ERROR)
		GLES2_ERROR(GL_INVALID_ENUM)
		GLES2_ERROR(GL_INVALID_VALUE)
		GLES2_ERROR(GL_INVALID_OPERATION)
		GLES2_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION)
		GLES2_ERROR(GL_OUT_OF_MEMORY)
	}
#undef GLES2_ERROR
	return "UNKNOWN(GLES2)";
}

GLenum implement_GL_CHECK(const char* msg, const char* file, int line)
{
	GLenum first_glError = ctx.glGetError();
	GLenum glError = first_glError;

	if(glError == GL_NO_ERROR)
	{
		return GL_NO_ERROR;
	}

	serrf(
		"GL_CHECK failed: %s\n"
		"File: %s\n"
		"Line: %i\n",
		(msg == NULL ? "" : msg),
		file,
		line);

	do
	{
		serrf("glGetError() = %s (0x%.8x)\n", gl_err_string(glError), glError);
		glError = ctx.glGetError();
	} while(glError != GL_NO_ERROR);

	return first_glError;
}

static const char* GetGLDebugSeverityKHR(GLenum severity)
{
#define GL_DEBUG_SEVERITY(x) \
	case(GL_DEBUG_SEVERITY_##x##_KHR): return (#x);
	switch(severity)
	{
		GL_DEBUG_SEVERITY(HIGH)
		GL_DEBUG_SEVERITY(MEDIUM)
		GL_DEBUG_SEVERITY(LOW)
		GL_DEBUG_SEVERITY(NOTIFICATION)
	}
#undef GL_DEBUG_SEVERITY
	return "UNKNOWN";
}

static const char* GetGLDebugTypeKHR(GLenum type)
{
#define GL_DEBUG_TYPE(x) \
	case(GL_DEBUG_TYPE_##x##_KHR): return (#x);
	switch(type)
	{
		GL_DEBUG_TYPE(DEPRECATED_BEHAVIOR)
		GL_DEBUG_TYPE(ERROR)
		GL_DEBUG_TYPE(MARKER)
		GL_DEBUG_TYPE(OTHER)
		GL_DEBUG_TYPE(PERFORMANCE)
		GL_DEBUG_TYPE(POP_GROUP)
		GL_DEBUG_TYPE(PUSH_GROUP)
		GL_DEBUG_TYPE(UNDEFINED_BEHAVIOR)
	}
#undef GL_DEBUG_TYPE
	return "UNKNOWN";
}

// from https://github.com/nvMcJohn/apitest
// I am keeping the link because I want to look at the code as a reference
// --------------------------------------------------------------------------------------------------------------------
static void ErrorCallback(
	GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* userParam)
{
	(void)userParam;
	(void)length;
	(void)source;
	(void)id;

	if(type != GL_DEBUG_TYPE_ERROR_KHR && type != GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR)
	{
		slogf(
			"\nGL CALLBACK: type = %s (0x%x), severity = %s (0x%x), message = %s\n",
			GetGLDebugTypeKHR(type),
			type,
			GetGLDebugSeverityKHR(severity),
			severity,
			message);
	}
	else
	{
		serrf(
			"\nGL CALLBACK: type = %s (0x%x), severity = %s (0x%x), message = %s\n",
			GetGLDebugTypeKHR(type),
			type,
			GetGLDebugSeverityKHR(severity),
			severity,
			message);

		static bool only_once = false;
		if(!only_once || cv_debug_opengl.data == 2.0)
		{
			only_once = true;
			std::string stack_message;
			debug_str_stacktrace(&stack_message, 0);
			serrf(
				"\nGL StackTrace:\n"
				"%s\n",
				stack_message.c_str());
		}
	}
}

#if 1 // def _WIN32
NDSERR static void* wrapper_SDL_GL_GetProcAddress(const char* name)
{
	void* func = SDL_GL_GetProcAddress(name);
	if(func == NULL)
	{
		serrf("Couldn't load GLES2 function %s: %s", name, SDL_GetError());
		return NULL;
	}
	return func;
}
#endif

bool LoadGLContext(GLES2_Context* data)
{
#if 0 // ndef _WIN32
#define SDL_PROC(ret, func, params) data->func = func;
#define NULL_PROC(ret, func, params) data->func = func;
#else
#define SDL_PROC(ret, func, params)                                                            \
	data->func = reinterpret_cast<decltype(data->func)>(wrapper_SDL_GL_GetProcAddress(#func)); \
	if(data->func == NULL) return false;
#define NULL_PROC(ret, func, params) \
	data->func = reinterpret_cast<decltype(data->func)>(SDL_GL_GetProcAddress(#func));
#endif

#include "SDL_gles2funcs.h.txt"

#undef NULL_PROC
#undef SDL_PROC

	if(cv_debug_opengl.data == 1)
	{
		if(SDL_GL_ExtensionSupported("GL_KHR_debug") == SDL_FALSE)
		{
			slog("warning cv_opengl_debug: GL_KHR_debug unsupported\n");
		}
		else
		{
			ASSERT(ctx.glDebugMessageControl);
			ASSERT(ctx.glDebugMessageCallback);
			ctx.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			ctx.glDebugMessageCallback(ErrorCallback, nullptr);
			ctx.glEnable(GL_DEBUG_OUTPUT_KHR);
			cv_has_GL_KHR_debug.data = 1;
		}
	}

	if(SDL_GL_ExtensionSupported("GL_EXT_disjoint_timer_query") == SDL_FALSE)
	{
		slog("warning GL_EXT_disjoint_timer_query unsupported\n");
	}
	else
	{
		ASSERT(ctx.glGenQueriesEXT != NULL);
		ASSERT(ctx.glDeleteQueriesEXT != NULL);
		ASSERT(ctx.glBeginQueryEXT != NULL);
		ASSERT(ctx.glEndQueryEXT != NULL);
		ASSERT(ctx.glGetQueryivEXT != NULL);
		ASSERT(ctx.glQueryCounterEXT != NULL);
		ASSERT(ctx.glGetQueryObjectivEXT != NULL);
		ASSERT(ctx.glGetQueryObjectui64vEXT != NULL);
		cv_has_EXT_disjoint_timer_query.data = 1;
	}

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

NDSERR static GLuint gl_compile_shader(
	const char* file_info, int shader_count, const GLchar* const* shader_script, GLenum type)
{
	ASSERT(file_info != NULL);
	ASSERT(shader_script != NULL);
	ASSERT(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);

	GLuint shader_id;
	shader_id = ctx.glCreateShader(type);
	if(shader_id == 0)
	{
		serrf("<%s> error: glCreateShader returned 0\n", file_info);
		return 0;
	}

	ctx.glShaderSource(shader_id, shader_count, shader_script, NULL);
	ctx.glCompileShader(shader_id);

	GLint compile_status;
	ctx.glGetShaderiv(shader_id, GL_COMPILE_STATUS, &compile_status);

	GLint log_length;
	GLint infoLogLength;

	ctx.glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);
	if(log_length > 1)
	{
		std::unique_ptr<char[]> message(new char[log_length]);
		ctx.glGetShaderInfoLog(shader_id, log_length, &infoLogLength, message.get());
		serrf(
			"<%s> %s: %s\n",
			file_info,
			(compile_status != GL_TRUE ? "error" : "warn"),
			message.get());
	}

	if(compile_status != GL_TRUE)
	{
		ctx.glDeleteShader(shader_id);
		return 0;
	}

	return shader_id;
}

GLuint gl_create_program(
	const char* vert_info,
	const GLchar* vert_shader,
	const char* frag_info,
	const GLchar* frag_shader)
{
	return gl_create_program2(vert_info, 1, &vert_shader, frag_info, 1, &frag_shader);
}

// gl_create_program but you can use an array of strings.
GLuint gl_create_program2(
	const char* vert_info,
	int vert_count,
	const GLchar* const* vert_shader,
	const char* frag_info,
	int frag_count,
	const GLchar* const* frag_shader)
{
	GLuint program_id = 0;
	GLuint vertex_id = 0;
	GLuint fragment_id = 0;
	do
	{
		vertex_id = gl_compile_shader(vert_info, vert_count, vert_shader, GL_VERTEX_SHADER);
		if(vertex_id == 0)
		{
			break;
		}

		fragment_id = gl_compile_shader(frag_info, frag_count, frag_shader, GL_FRAGMENT_SHADER);
		if(fragment_id == 0)
		{
			break;
		}

		program_id = ctx.glCreateProgram();
		if(program_id == 0)
		{
			serrf("<vert:%s><frag:%s> error: glCreateProgram returned 0\n", vert_info, frag_info);
			break;
		}

		ctx.glAttachShader(program_id, vertex_id);
		ctx.glAttachShader(program_id, fragment_id);

		ctx.glLinkProgram(program_id);

		GLint link_status;
		ctx.glGetProgramiv(program_id, GL_LINK_STATUS, &link_status);

		GLint log_length;
		GLint infoLogLength;

		ctx.glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length > 1)
		{
			std::unique_ptr<char[]> message(new char[log_length]);
			ctx.glGetProgramInfoLog(program_id, log_length, &infoLogLength, message.get());
			serrf(
				"<vert:%s><frag:%s> %s: %s\n",
				vert_info,
				frag_info,
				(link_status != GL_TRUE ? "error" : "warn"),
				message.get());
		}

		if(link_status != GL_TRUE)
		{
			ctx.glDeleteProgram(program_id);
			program_id = 0;
			break;
		}

	} while(false);
	if(vertex_id != 0)
	{
		ctx.glDeleteShader(vertex_id);
	}
	if(fragment_id != 0)
	{
		ctx.glDeleteShader(fragment_id);
	}
	return program_id;
}
