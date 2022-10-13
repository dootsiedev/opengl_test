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
#endif


enum class CONSOLE_MESSAGE_TYPE
{
	INFO,
	ERROR
};

struct log_queue
{
    struct log_message
	{
		// the reason I use a unique_ptr is because
		// my log system "slog" allocates one from my unique_asprintf
		// so I can just move it in easily (maybe I should use C++20's std::string feature)
		log_message() = default;
		log_message(
			CONSOLE_MESSAGE_TYPE type_, std::unique_ptr<char[]> message_, int message_length_)
		: type(type_)
		, message_length(message_length_)
		, message(std::move(message_))
		{
		}
		CONSOLE_MESSAGE_TYPE type;
		int message_length;
		std::unique_ptr<char[]> message;
		// probably could add in time if I wanted.
	};

#ifndef __EMSCRIPTEN__
	// the queue's mutex
	std::mutex mut;
#endif

    // you are supposed to just access this member .emplace_back to add a log.
	// I could also try to replace this with a circular buffer (or whatever),
	// since I want to remove old messages anyways.
	std::deque<log_message> message_queue;
};

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

	std::array<text_prompt_wrapper::color_pair, 3> log_color_table = {
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

extern log_queue g_log;