#pragma once

#include "global.h"

#include "opengles2/opengl_stuff.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"
#include "BS_Archive/BS_archive.h"

#include <SDL2/SDL.h>
#include <deque>
#include <memory>

// I don't use threads on emscripten.
#ifndef __EMSCRIPTEN__
#include <mutex>
#include <atomic>
#endif

enum class CONSOLE_MESSAGE_TYPE
{
	INFO,
	ERROR
};

struct log_queue
{
	enum
	{
		MESSAGE_COUNT_SIZE = 100,
		MESSAGE_BUFFER_SIZE = 10000,
	};
	struct log_message
	{
		size_t count; // the number of characters written (the return value of fprintf)
		CONSOLE_MESSAGE_TYPE type;
	};

	// there are 2 buffers, which get swapped when filled.
	// if a buffer cannot swap because the other buffer has messages,
	// the buffer's messages will be erased.
	// the reason why I did this is not for multithreading
	// but because this implementation is more simple than a circular buffer.
	struct log_buffer
	{
		size_t message_count = 0;
		size_t messages_read = 0;
		size_t write_cursor = 0;
		size_t read_cursor = 0;
		log_message entries[MESSAGE_COUNT_SIZE];
		char data[MESSAGE_BUFFER_SIZE];
	};

	size_t buf_write_id = 0;
	size_t buf_read_id = 0;
	log_buffer buffers[2];

	void push_raw(CONSOLE_MESSAGE_TYPE type, const char* str, size_t len);
	void push_vargs(CONSOLE_MESSAGE_TYPE type, const char* fmt, va_list args);
	const char* pop(log_message* message);
};

#ifndef __EMSCRIPTEN__
// the queue's mutex
extern std::mutex g_log_mut;
#endif
extern log_queue g_log;

enum class CONSOLE_RESULT
{
	CONTINUE,
	ERROR
};

struct console_state
{
	// keep track of how many newlines are drawn
	// to cut lines from the top when the limit is reached.
	int log_line_count = 0;

	std::array<text_prompt_wrapper::color_pair, 1> log_color_table = {
		text_prompt_wrapper::color_pair{{255, 255, 255, 255}, RGBA8_PREMULT(255, 0, 0, 200)}};

	// it's possible to use one VBO at the expense of
	// re-drawing everything for any modification.
	font_style_interface* console_font = NULL;
	mono_2d_batcher* console_batcher = NULL;

	// the log of messages
	text_prompt_wrapper log_box;
	GLuint gl_log_interleave_vbo = 0;
	GLuint gl_log_vao_id = 0;
	GLsizei log_vertex_count = 0;

	// the text you type into
	text_prompt_wrapper prompt_cmd;
	GLuint gl_prompt_interleave_vbo = 0;
	GLuint gl_prompt_vao_id = 0;
	GLsizei prompt_vertex_count = 0;

	// this is the last error that was printed
	// put into a static area so you can read it.
	text_prompt_wrapper error_text;
	GLuint gl_error_interleave_vbo = 0;
	GLuint gl_error_vao_id = 0;
	GLsizei error_vertex_count = 0;

	const char* history_path = "console_hist.json";
	std::deque<std::string> command_history;
	// when you press up, you still want to restore the original prompt
	std::string original_prompt;
	int history_index = -1;

	NDSERR bool init(
		font_style_interface* console_font_,
		mono_2d_batcher* console_batcher_,
		shader_mono_state& mono_shader);
	NDSERR bool destroy();

	NDSERR CONSOLE_RESULT input(SDL_Event& e);

	void resize_text_area();

	NDSERR bool parse_input();

	// this checks for new logs
	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	// put the error on the error section.
	// this is a nice place to put a non-fatal serr error
	void post_error(std::string_view msg);

	void serialize_history(BS_Archive& ar);
};