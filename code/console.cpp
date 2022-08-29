#include "global.h"

#include "console.h"

#include "app.h"
#include "font/utf8_stuff.h"
#include "cvar.h"
#include "BS_Archive/BS_json.h"
#include "BS_Archive/BS_stream.h"

#include <SDL2/SDL.h>

console_state g_console;

static REGISTER_CVAR_INT(
	cv_console_history_max, 100, "the maximum number of commands in the history (up arrow)", CVAR_T::RUNTIME);
static REGISTER_CVAR_INT(
	cv_console_log_max_row_count, 100, "the maximum number of rows shown in the log", CVAR_T::RUNTIME);


bool console_state::init(font_bitmap_cache* font_style, shader_mono_state& mono_shader)
{
	ASSERT(font_style);
	font_manager = font_style->font_manager;

	//
	// log
	//

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_log_interleave_vbo);
	if(gl_log_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// VAO
	ctx.glGenVertexArrays(1, &gl_log_vao_id);
	if(gl_log_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	// vertex setup
	ctx.glBindVertexArray(gl_log_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_log_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);

	log_batcher.SetFont(font_style);

	// this requires the atlas texture to be bound with 1 byte packing
	if(!log_box.init(
		   "",
		   &log_batcher,
		   TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY |
			   TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

	//
	// prompt
	//

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_prompt_interleave_vbo);
	if(gl_prompt_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// VAO
	ctx.glGenVertexArrays(1, &gl_prompt_vao_id);
	if(gl_prompt_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	// vertex setup
	ctx.glBindVertexArray(gl_prompt_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_prompt_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);

	prompt_batcher.SetFont(font_style);

	if(!prompt_cmd.init(
		   "",
		   &prompt_batcher,
		   TEXTP_SINGLE_LINE | TEXTP_X_SCROLL | TEXTP_DRAW_BBOX | TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

	//
	// error
	//

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_error_interleave_vbo);
	if(gl_error_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// VAO
	ctx.glGenVertexArrays(1, &gl_error_vao_id);
	if(gl_error_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	// vertex setup
	ctx.glBindVertexArray(gl_error_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_error_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);

	error_batcher.SetFont(font_style);

	if(!error_text.init(
		   "", &error_batcher, TEXTP_READ_ONLY | TEXTP_DISABLE_CULL | TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

	// set to red.
	error_text.text_color = {255, 255, 255, 255};
	error_text.backdrop_color = RGBA8_PREMULT(255, 0, 0, 200);

	//
	// finish
	//
	ctx.glBindVertexArray(0);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

	// set the area the text is placed.
	resize_text_area();

	FILE* fp = fopen(history_path, "rb");
	if(fp == NULL)
	{
		// not existing is not an error (maybe make an info log?)
		// ENOENT = No such file or directory
		if(errno != ENOENT)
		{
			serrf("Failed to open: `%s`, reason: %s\n", history_path, strerror(errno));
			// put the message into the console instead
			if(!error_text.replace_string(serr_get_error()))
			{
				return false;
			}
		}
	}
	else
	{
		RWops_Stdio history_file(fp, history_path);
		char buffer[1000];
		BS_ReadStream sb(&history_file, buffer, sizeof(buffer));
		BS_JsonReader ar(sb);
		serialize_history(ar);
		if(!ar.Finish(history_file.name()))
		{
			// put the message into the console instead
			if(!error_text.replace_string(serr_get_error()))
			{
				return false;
			}
		}
	}

	return GL_CHECK(__func__) == GL_NO_ERROR;
}
bool console_state::destroy()
{
	font_manager = NULL;

	SAFE_GL_DELETE_VBO(gl_log_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_log_vao_id);
	SAFE_GL_DELETE_VBO(gl_prompt_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_prompt_vao_id);
	SAFE_GL_DELETE_VBO(gl_error_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_error_vao_id);

	FILE* fp = serr_wrapper_fopen(history_path, "wb");
	if(fp == NULL)
	{
		return false;
	}

	if(command_history.size() > static_cast<size_t>(cv_console_history_max.data))
	{
		command_history.resize(cv_console_history_max.data);
	}

	RWops_Stdio history_file(fp, history_path);
	char buffer[1000];
	BS_WriteStream sb(&history_file, buffer, sizeof(buffer));
	BS_JsonWriter ar(sb);
	serialize_history(ar);
	if(!ar.Finish(history_file.name()))
	{
		return false;
	}

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

void console_state::serialize_history(BS_Archive& ar)
{
	ar.StartObject();

	ar.Key("size");
	size_t test_array_size = std::size(command_history);
	if(!ar.Uint64(test_array_size))
	{
		// exit early
		return;
	}

	if(ar.IsReader())
	{
		command_history.resize(test_array_size);
	}

	ar.Key("history");
	ar.StartArray();
	for(std::string& entry : command_history)
	{
		if(!ar.Good())
		{
			// exit early
			return;
		}
		ar.String(entry);
	}
	ar.EndArray();
	ar.EndObject();
}

void console_state::resize_text_area()
{
	log_box.set_bbox(
		60,
		60,
		static_cast<float>(cv_screen_width.data) / 2 - 60,
		static_cast<float>(cv_screen_height.data) / 2 - 60);
	prompt_cmd.set_bbox(
		60,
		60 + static_cast<float>(cv_screen_height.data) / 2 - 60 + 10.f,
		static_cast<float>(cv_screen_width.data) / 2 - 60,
		prompt_batcher.GetLineSkip());

	// this probably doesn't have enough hieght to fit in messages with stack traces,
	// but if it's too big it could potentially block UI elements in an annoying way
	error_text.set_bbox(
		60,
		prompt_cmd.box_ymax + 10.f,
		static_cast<float>(cv_screen_width.data) / 2 - 60,
		prompt_batcher.GetLineSkip() * 10);
}

CONSOLE_RESULT console_state::input(SDL_Event& e)
{
	ASSERT(font_manager != NULL);

    if(prompt_cmd.text_focus)
	{
		switch(e.type)
		{
		case SDL_KEYDOWN:
			switch(e.key.keysym.sym)
			{
			case SDLK_RETURN:
				if(!parse_input())
				{
					return CONSOLE_RESULT::ERROR;
				}
				return CONSOLE_RESULT::EAT;
			case SDLK_UP:
				// show old commands like a terminal
				if(history_index == -1 && !command_history.empty())
				{
					original_prompt = prompt_cmd.get_string();
					history_index = 0;
					if(!prompt_cmd.replace_string(command_history.at(0)))
					{
						return CONSOLE_RESULT::ERROR;
					}
				}
				else if(static_cast<size_t>(history_index) < command_history.size() - 1)
				{
					history_index++;
					if(!prompt_cmd.replace_string(command_history.at(history_index)))
					{
						return CONSOLE_RESULT::ERROR;
					}
				}
				return CONSOLE_RESULT::EAT;

			case SDLK_DOWN:
				// show newer commands like a terminal
				if(history_index == -1)
				{
					break;
				}
				if(history_index == 0)
				{
					if(!prompt_cmd.replace_string(original_prompt))
					{
						return CONSOLE_RESULT::ERROR;
					}
					original_prompt.clear();
					history_index = -1;
				}
				else
				{
					history_index--;
					if(!prompt_cmd.replace_string(command_history.at(history_index)))
					{
						return CONSOLE_RESULT::ERROR;
					}
				}
				return CONSOLE_RESULT::EAT;
			}
			break;
		}
	}

    // if we eat the input, unfocus the other elements
	bool input_eaten = false;

	switch(log_box.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::EAT: input_eaten = true; break;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	if(input_eaten)
	{
		prompt_cmd.unfocus();
	}
	else
	{
		switch(prompt_cmd.input(e))
		{
		case TEXT_PROMPT_RESULT::CONTINUE: break;
		case TEXT_PROMPT_RESULT::EAT: input_eaten = true; break;
		case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
		}
	}

	if(input_eaten)
	{
		error_text.unfocus();
	}
	else
	{
		// Only parse input when there is actual content.
		// possible because this is read only.
		if(error_batcher.vertex_count() != 0)
		{
			switch(error_text.input(e))
			{
			case TEXT_PROMPT_RESULT::CONTINUE: break;
			case TEXT_PROMPT_RESULT::EAT: input_eaten = true; break;
			case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
			}
		}
	}

	return input_eaten ? CONSOLE_RESULT::EAT : CONSOLE_RESULT::CONTINUE;
}

bool console_state::parse_input()
{
	// TODO: this is way too simple, it can't support escape keys or quotes.
	// The quick and ugly solution would be to use boost spirit.
	std::string line = prompt_cmd.get_string();
	prompt_cmd.clear_string();

	// show the command in the log
	slogf("%s\n", line.c_str());

    // put the message into the history.
	if(!line.empty())
	{
		command_history.push_front(line);
		if(command_history.size() > static_cast<size_t>(cv_console_history_max.data))
		{
			command_history.pop_back();
			history_index = std::min(history_index, cv_console_history_max.data - 1);
		}
	}
	history_index = -1;
	
    // this will modify the string which means you can't use line after this.
    if(!cvar_line(CVAR_T::RUNTIME, line.data()))
    {
        return error_text.replace_string(serr_get_error());
    }
    
    return true;
}

bool console_state::draw()
{
	ASSERT(font_manager != NULL);
	bool success = true;

	log_message message_buffer[100];
	size_t message_count = 0;
	{
		std::lock_guard<std::mutex> lk(mut);
		while(!message_queue.empty() && message_count < std::size(message_buffer))
		{
			message_buffer[message_count++] = std::move(message_queue.front());
			message_queue.pop_front();
		}
	}

	if(message_count != 0)
	{
		// text_prompt_wrapper::prompt_char
		STB_TEXTEDIT_CHARTYPE text_data[10000];
		size_t char_count = 0;
		for(size_t i = 0; i < message_count; ++i)
		{
			const char* str_cur = message_buffer[i].message.get();
			const char* str_end =
				message_buffer[i].message.get() + message_buffer[i].message_length;
			while(str_cur != str_end && char_count < std::size(text_data))
			{
				uint32_t codepoint;
				utf8::internal::utf_error err_code =
					utf8::internal::validate_next(str_cur, str_end, codepoint);
				if(err_code != utf8::internal::UTF8_OK)
				{
					serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
					return false;
				}
				// this would be faster if I had access to GetAdvance, & newline width
				// but performance isn't a goal.
				// TODO: I want to make errors be colored, but the text_prompt_wrapper can't ATM
				// note this will break undo / redo! (doesn't matter because read-only)
				text_data[char_count++] = codepoint;

                if(codepoint == '\n')
                {
                    log_line_count++;
                }
			}
		}

		log_box.set_readonly(false);
		// this requires the atlas texture to be bound with 1 byte packing
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		log_box.stb_insert_chars(log_box.text_data.size(), text_data, char_count);
		log_box.set_readonly(true);

		// TODO: I don't always want this to scroll to the bottom,
		// I would like it to only do that when the scrollbar is already at the bottom!
		log_box.scroll_to_bottom();

        if(log_line_count > cv_console_log_max_row_count.data)
        {
			auto trim_cursor = log_box.text_data.begin();
			auto trim_end = log_box.text_data.end();
			for(; log_line_count > cv_console_log_max_row_count.data; log_line_count--)
			{
				for(; trim_cursor != trim_end && (trim_cursor++)->codepoint != '\n'; )
				{
				}
			}
			log_box.set_readonly(false);
			// NOLINTNEXTLINE(bugprone-narrowing-conversions)
			log_box.stb_delete_chars(0, std::distance(log_box.text_data.begin(), trim_cursor));
			log_box.set_readonly(true);
        }
	}
	if(!success)
	{
		return false;
	}

	if(log_box.draw_requested())
	{
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_log_interleave_vbo);
		log_batcher.begin();
		// this requires the atlas texture to be bound with 1 byte packing
		if(!log_box.draw())
		{
			return false;
		}
		log_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	if(prompt_cmd.draw_requested())
	{
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_prompt_interleave_vbo);
		prompt_batcher.begin();
		// this requires the atlas texture to be bound with 1 byte packing
		if(!prompt_cmd.draw())
		{
			return false;
		}
		prompt_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	if(error_text.draw_requested())
	{
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_error_interleave_vbo);
		error_batcher.begin();
		// this requires the atlas texture to be bound with 1 byte packing
		if(!error_text.draw())
		{
			return false;
		}
		error_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void console_state::unfocus()
{
	prompt_cmd.unfocus();
	log_box.unfocus();
	error_text.unfocus();
}
void console_state::focus()
{
	prompt_cmd.focus();
}
