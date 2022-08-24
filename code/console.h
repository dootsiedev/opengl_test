#pragma once

#include "global.h"

#include "opengles2/opengl_stuff.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"


#include <SDL2/SDL.h>
#include <deque>
#include <memory>
#include <mutex>

enum class CONSOLE_RESULT
{
    EAT,
	CONTINUE,
	ERROR
};

enum class CONSOLE_MESSAGE_TYPE
{
    INFO,
    ERROR
};

struct console_state
{
	struct log_message
	{
		// the reason I use a unique_ptr is because
		// my log system "slog" will allocate a unique_ptr for my asprintf formatter
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

	// you are supposed to just access this member .emplace_back to add a log.
	// I could also try to replace this with a circular buffer (or whatever), 
    // since I want to remove old messages anyways.
	std::deque<log_message> message_queue;

    // the queue's mutex
    std::mutex mut;

	font_manager_state* font_manager = NULL;
	GLuint gl_log_interleave_vbo = 0;
	GLuint gl_log_vao_id = 0;
    font_sprite_batcher log_batcher;
    text_prompt_wrapper log;

    GLuint gl_prompt_interleave_vbo = 0;
    GLuint gl_prompt_vao_id = 0;
    font_sprite_batcher prompt_batcher;
    text_prompt_wrapper prompt;


    // this requires the atlas texture to be bound with 1 byte packing
    bool init(font_bitmap_cache* font_style, shader_mono_state& mono_shader);
    bool destroy();

    // this requires the atlas texture to be bound with 1 byte packing
    CONSOLE_RESULT input(SDL_Event& e);

	void parse_input();

	// this won't render to the framebuffer, 
    // this will just modify the atlas and buffer data.
    // you need to access log_batcher and prompt_batcher directly to render.
    // this requires the atlas texture to be bound with 1 byte packing
    bool draw();

    // call this when you need to unfocus, like for example if you press escape or something.
    void unfocus()
    {
        prompt.unfocus();
        log.unfocus();
    }
    void focus()
    {
        prompt.focus();
    }
};


extern console_state g_console;