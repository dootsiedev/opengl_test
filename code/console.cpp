#include "global_pch.h"
#include "global.h"

#include "console.h"

#include "app.h"
#include "font/utf8_stuff.h"
#include "cvar.h"
#include "BS_Archive/BS_json.h"
#include "BS_Archive/BS_stream.h"
#include "ui.h"

#include <SDL2/SDL.h>

// TODO(dootsie): really needs a expand button/hotkey...
// TODO(dootsie): a option for the default size / orientation
// TODO(dootsie): add a mode/button where I disable the mouse input, and and make everything more
// transparent and maybe what I could do with this mode is display errors,
// and maybe make it so that "console handled errors" will be shown in the "error box",
// and "handled" errors (like a error manully presented through some UI) will not overwrite the box.

log_queue g_log;

static CVAR_T history_enabled
#ifndef __EMSCRIPTEN__
	= CVAR_T::RUNTIME;
#else
	= CVAR_T::DISABLED;
#endif

static REGISTER_CVAR_INT(
	cv_console_history_max,
	100,
	"the maximum number of commands in the history (up arrow)",
	history_enabled);
static REGISTER_CVAR_INT(
	cv_console_log_max_row_count,
	500,
	"the maximum number of rows shown in the log",
	CVAR_T::RUNTIME);

bool console_state::init(
	font_style_interface* console_font_,
	mono_2d_batcher* console_batcher_,
	shader_mono_state& mono_shader)
{
	ASSERT(console_font_ != NULL);
	ASSERT(console_batcher_ != NULL);

	console_font = console_font_;
	console_batcher = console_batcher_;

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

	if(!log_box.init(
		   "",
		   console_batcher,
		   console_font,
		   TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY |
			   TEXTP_DRAW_BACKDROP))
	{
		return false;
	}
	/*
	if(!log_box.set_scale(2))
	{
		return false;
	}
	*/
	// set the color table so we can print errors with a different color
	log_box.color_table = log_color_table.data();
	log_box.color_table_size = log_color_table.size();

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

	if(!prompt_cmd.init(
		   "",
		   console_batcher,
		   console_font,
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

	if(!error_text.init(
		   "",
		   console_batcher,
		   console_font,
		   TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY |
			   TEXTP_DRAW_BACKDROP))
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

#ifndef __EMSCRIPTEN__
	FILE* fp = fopen(history_path, "rb");
	if(fp == NULL)
	{
		// not existing is not an error (maybe make an info log?)
		// ENOENT = No such file or directory
		if(errno != ENOENT)
		{
			serrf("Failed to open: `%s`, reason: %s\n", history_path, strerror(errno));
			// put the message into the console instead
			post_error(serr_get_error());
		}
	}
	else
	{
		// in hindsight the one downside of using JSON is that it would be better
		// to just append the file + flush than writing the whole json for every command
		// if I wanted to support the ability to save the history even after a segfault.
		RWops_Stdio history_file(fp, history_path);
		char buffer[1000];
		BS_ReadStream sb(&history_file, buffer, sizeof(buffer));
		BS_JsonReader ar(sb);
		serialize_history(ar);
		if(!ar.Finish(history_file.name()))
		{
			// put the message into the console instead
			post_error(serr_get_error());
		}
		if(!history_file.close())
		{
			post_error(serr_get_error());
		}
	}
#endif

	return GL_CHECK(__func__) == GL_NO_ERROR;
}
bool console_state::destroy()
{
	SAFE_GL_DELETE_VBO(gl_log_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_log_vao_id);
	SAFE_GL_DELETE_VBO(gl_prompt_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_prompt_vao_id);
	SAFE_GL_DELETE_VBO(gl_error_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_error_vao_id);

	log_line_count = 0;
	log_box.clear_string();
	prompt_cmd.clear_string();
	error_text.clear_string();

	bool success = true;

#ifndef __EMSCRIPTEN__

	if(!command_history.empty())
	{
		FILE* fp = serr_wrapper_fopen(history_path, "wb");
		if(fp == NULL)
		{
			success = false;
		}
		else
		{
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
				success = false;
			}
			if(!history_file.close())
			{
				success = false;
			}
		}
	}
#endif

	return GL_CHECK(__func__) == GL_NO_ERROR && success;
}

void console_state::serialize_history(BS_Archive& ar)
{
	ar.StartObject();

	ar.Key("size");
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	uint32_t test_array_size = std::size(command_history);
	if(!ar.Uint32(test_array_size))
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
	float width = static_cast<float>(cv_screen_width.data) / 2 - 60;
	log_box.set_bbox(60, 60, width, static_cast<float>(cv_screen_height.data) / 2 - 60);
	// TODO(dootsie): I don't like this, it should only move down when it already is at the
	// bottom...
	log_box.scroll_to_bottom();
	prompt_cmd.set_bbox(60, log_box.box_ymax + 10.f, width, prompt_cmd.get_lineskip() + 1);

	// this probably doesn't have enough hieght to fit in messages with stack traces,
	// but if it's too big it could potentially block UI elements in an annoying way
	error_text.set_bbox(
		60,
		prompt_cmd.box_ymax + 10.f,
		width,
		static_cast<float>(cv_screen_height.data) - 60 - (prompt_cmd.box_ymax + 10.f));
}

CONSOLE_RESULT console_state::input(SDL_Event& e)
{
	if(e.type == SDL_WINDOWEVENT)
	{
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_SIZE_CHANGED: resize_text_area(); break;
		}
	}
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
				// eat
				set_event_unfocus(e);
				return CONSOLE_RESULT::CONTINUE;
			case SDLK_UP:
				if((e.key.keysym.mod & KMOD_SHIFT) != 0)
				{
					break;
				}
				// show old commands like a terminal
				if(history_index == -1 && !command_history.empty())
				{
					original_prompt = prompt_cmd.get_string();
					history_index = 0;
					prompt_cmd.replace_string(command_history.at(0));
				}
				else if(static_cast<size_t>(history_index) < command_history.size() - 1)
				{
					history_index++;
					prompt_cmd.replace_string(command_history.at(history_index));
				}
				// eat
				set_event_unfocus(e);
				return CONSOLE_RESULT::CONTINUE;

			case SDLK_DOWN:
				if((e.key.keysym.mod & KMOD_SHIFT) != 0)
				{
					break;
				}
				// show newer commands like a terminal
				if(history_index != -1)
				{
					if(history_index == 0)
					{
						prompt_cmd.replace_string(original_prompt);
						original_prompt.clear();
						history_index = -1;
					}
					else
					{
						history_index--;
						prompt_cmd.replace_string(command_history.at(history_index));
					}
				}
				// eat
				set_event_unfocus(e);
				return CONSOLE_RESULT::CONTINUE;
			}
			break;
		}
	}

	switch(log_box.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE:
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::UNFOCUS: break;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	switch(prompt_cmd.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE:
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::UNFOCUS: break;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}
	// Only parse input when there is actual content.
	// possible because this is read only.
	if(!error_text.text_data.empty())
	{
		switch(error_text.input(e))
		{
		case TEXT_PROMPT_RESULT::CONTINUE:
		case TEXT_PROMPT_RESULT::MODIFIED:
		case TEXT_PROMPT_RESULT::UNFOCUS: break;
		case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
		}
	}

	return CONSOLE_RESULT::CONTINUE;
}

bool console_state::parse_input()
{
	std::string line = prompt_cmd.get_string();
	prompt_cmd.clear_string();

	// show the command in the log
	slogf("%s\n", line.c_str());

	// put the message into the history.
	if(!line.empty())
	{
		if(!command_history.empty() && line == command_history.front())
		{
			// no duplicate history entries.
		}
		else
		{
			command_history.push_front(line);
			// only cull the history when writing the file.
			/*
			if(command_history.size() > static_cast<size_t>(cv_console_history_max.data))
			{
				command_history.pop_back();
				history_index = std::min(history_index, cv_console_history_max.data - 1);
			}
			*/
		}
	}
	history_index = -1;

	// TODO: save options?
	if(line == "help")
	{
		// TODO: probably should have an option to only show runtime options?
		// and also showing the debug lines would be nice too.
		cvar_list(false);
	}
	else
	{
		// this will modify the string which means you can't use line after this.
		if(!cvar_line(CVAR_T::RUNTIME, line.data()))
		{
			slog("note, you can type \"help\" for a list of cvars.\n");
			// TODO: maybe instead of doing this here, I let all the errors
			// get posted to the console, from outside console(demo)?
			post_error(serr_get_error());
		}
	}

	// slogf("log text count: %zu\n", log_box.text_data.size());

	return true;
}

bool console_state::update(double delta_sec)
{
	// TODO(dootsie): these could be cvars.
	log_queue::log_message message_buffer[100];
	char text_buffer[10000];
	size_t message_count = 0;
	size_t bytes_to_read = 0;
	int queue_size = 0;
	// TIMER_U tick1;
	// TIMER_U tick2;
	{
		// tick1 = timer_now();
#ifndef __EMSCRIPTEN__
		// YOU CANNOT PRINT TO SLOG OR SERR IN THIS LOCK!!!
		std::lock_guard<std::mutex> lk(g_log.mut);
#endif
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		queue_size = g_log.message_queue.size();

		if(queue_size != 0)
		{
			int ret = fseek(get_global_log_file(), g_log.read_file_pos, SEEK_SET);
			(void)ret;
			ASSERT(ret == 0);
		}
		// since culling is based on newlines, I can assume every message has one line.
		if(queue_size > cv_console_log_max_row_count.data)
		{
			long seek_offset = 0; // NOLINT(google-runtime-int)
			auto erase_begin = g_log.message_queue.begin();
			auto erase_end = erase_begin + (queue_size - cv_console_log_max_row_count.data);
			for(auto it = erase_begin; it != erase_end; ++it)
			{
				seek_offset += it->count;
			}
			g_log.message_queue.erase(erase_begin, erase_end);
			int ret = fseek(get_global_log_file(), seek_offset, SEEK_CUR);
			(void)ret;
			ASSERT(ret == 0);
		}
		while(!g_log.message_queue.empty() && message_count < std::size(message_buffer))
		{
			// if there is no space, print what you can
			if(bytes_to_read + g_log.message_queue.front().count > std::size(text_buffer) - 1)
			{
				if(message_count > 0)
				{
                    // next pass you will get the whole message.
                    break;
                }
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				int space_remaining = (std::size(text_buffer) - 1) - bytes_to_read;
				if(space_remaining == 0)
				{
					// don't add a message with zero data
					break;
				}
				message_buffer[message_count++] = g_log.message_queue.front();
				message_buffer[message_count - 1].count = space_remaining;
				g_log.message_queue.front().count -= space_remaining;
				bytes_to_read = std::size(text_buffer) - 1;
				break;
			}
			bytes_to_read += g_log.message_queue.front().count;
			message_buffer[message_count++] = std::move(g_log.message_queue.front());
			g_log.message_queue.pop_front();
		}
		if(bytes_to_read > 0)
		{
			size_t nread = fread(text_buffer, 1, bytes_to_read, get_global_log_file());
            (void)nread;
			ASSERT(bytes_to_read == nread);

			// I don't need the null, but I prefer keeping it for the debugger.
			text_buffer[bytes_to_read] = '\0';

			long prev_pos = g_log.read_file_pos; // NOLINT(google-runtime-int)
			(void)prev_pos;
			g_log.read_file_pos = ftell(get_global_log_file());
			ASSERT(g_log.read_file_pos != -1);

			// set the steam to write at the end
			long ret = fseek(get_global_log_file(), 0, SEEK_END); // NOLINT(google-runtime-int)
			(void)ret;
			ASSERT(ret == 0);

#ifndef NDEBUG
			if(g_log.message_queue.empty())
			{
				long real_end_pos = ftell(get_global_log_file()); // NOLINT(google-runtime-int)
				ASSERT(real_end_pos != -1);
				// this means something go written into the log file without a properly notifying
				// the console.
				ASSERT(real_end_pos == g_log.read_file_pos);
			}
#endif
		}

		// tick2 = timer_now();
	}
	// slogf("console time: %g\n",timer_delta_ms(tick1, tick2));
	// can't print inside mutex because it would cause a deadlock.
	if(queue_size > cv_console_log_max_row_count.data)
	{
		// this is not an accurate representation of culling,
		// since there is a second culling pass that scans the newlines.
		slogf(
			"info: console culled messages: %d\n",
			(queue_size - cv_console_log_max_row_count.data));
	}

	if(message_count != 0)
	{
		STB_TEXTEDIT_CHARTYPE codepoint_buffer[std::size(text_buffer)];
		const char* str_cur = text_buffer;
		for(size_t i = 0; i < message_count; ++i)
		{
			size_t codepoint_count = 0;
			const char* str_end = str_cur + message_buffer[i].count;
			while(str_cur != str_end)
			{
				uint32_t codepoint;
				utf8::internal::utf_error err_code =
					utf8::internal::validate_next(str_cur, str_end, codepoint);
				if(err_code == utf8::internal::NOT_ENOUGH_ROOM ||
				   err_code == utf8::internal::INVALID_LEAD)
				{
					// this will happen when you use have a very large single message
                    // with unicode characters, and it overflows text_buffer.
					// this could solved by just using a VLA or alloca,
					// but only if you are happy with the possibility of a stack overflow.
					codepoint = static_cast<unsigned char>(*str_cur);
					++str_cur;
					// slogf("%s info: trunc log\n", __func__);
					// break;
				}
				else if(err_code != utf8::internal::UTF8_OK)
				{
					serrf("%s bad utf8: %s\n", __func__, cpputf_get_error(err_code));
					// put the message into the console instead
					post_error(serr_get_error());
					break;
				}
				if(codepoint_count >= std::size(codepoint_buffer) - 1)
				{
					slogf("%s info: trunc log\n", __func__);
					break;
				}
				codepoint_buffer[codepoint_count++] = codepoint;

				if(codepoint == '\n')
				{
					log_line_count++;
				}
			}
			switch(message_buffer[i].type)
			{
			case CONSOLE_MESSAGE_TYPE::INFO: log_box.current_color_index = 0; break;
			case CONSOLE_MESSAGE_TYPE::ERROR: log_box.current_color_index = 1; break;
			}
			// printf("char_count: %zu\n", char_count);
			// TODO(dootsie): I should probably include a newline before truncation...
			codepoint_buffer[codepoint_count] = '\0';

			log_box.set_readonly(false);
			// NOLINTNEXTLINE(bugprone-narrowing-conversions)
			log_box.stb_insert_chars(log_box.text_data.size(), codepoint_buffer, codepoint_count);
			log_box.set_readonly(true);
		}
		log_box.current_color_index = 0;

		// TODO(dootsie): I don't always want this to scroll to the bottom,
		// I would like it to only do that when the scrollbar is already at the bottom!
		log_box.scroll_to_bottom();

		// this probably isn't the fastest way of doing this.
		// I probably could implement this inside of text_prompt_wrapper itself
		// using some sort of sliding window thingy in pretext
		// and it would also fix the problem of this not being word-wrap aware.
		if(log_line_count > cv_console_log_max_row_count.data)
		{
			auto trim_cursor = log_box.text_data.begin();
			auto trim_end = log_box.text_data.end();
			for(; log_line_count > cv_console_log_max_row_count.data; log_line_count--)
			{
				for(; trim_cursor != trim_end && (trim_cursor++)->codepoint != '\n';)
				{
				}
			}
			log_box.set_readonly(false);
			// TODO: this causes the selection to be bugged out...
			// NOLINTNEXTLINE(bugprone-narrowing-conversions)
			log_box.stb_delete_chars(0, std::distance(log_box.text_data.begin(), trim_cursor));
			log_box.set_readonly(true);
		}
	}
	log_box.update(delta_sec);
	prompt_cmd.update(delta_sec);
	error_text.update(delta_sec);

	return true;
}

bool console_state::render()
{
	if(log_box.draw_requested())
	{
		console_batcher->clear();
		// this requires the atlas texture to be bound with 1 byte packing
		if(!log_box.draw())
		{
			// put the message into the console instead
			post_error(serr_get_error());
		}
		log_vertex_count = console_batcher->get_current_vertex_count();
		if(console_batcher->get_quad_count() != 0)
		{
			ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_log_interleave_vbo);
			ctx.glBufferData(
				GL_ARRAY_BUFFER, console_batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
			ctx.glBufferSubData(
				GL_ARRAY_BUFFER,
				0,
				console_batcher->get_current_vertex_size(),
				console_batcher->buffer);
			ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	if(prompt_cmd.draw_requested())
	{
		console_batcher->clear();
		// this requires the atlas texture to be bound with 1 byte packing
		if(!prompt_cmd.draw())
		{
			// put the message into the console instead
			post_error(serr_get_error());
		}
		prompt_vertex_count = console_batcher->get_current_vertex_count();
		if(console_batcher->get_quad_count() != 0)
		{
			ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_prompt_interleave_vbo);
			ctx.glBufferData(
				GL_ARRAY_BUFFER, console_batcher->get_total_vertex_size(), NULL, GL_STREAM_DRAW);
			ctx.glBufferSubData(
				GL_ARRAY_BUFFER,
				0,
				console_batcher->get_current_vertex_size(),
				console_batcher->buffer);
			ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}
	if(error_text.draw_requested())
	{
		// dont draw the bbox
		if(error_text.text_data.empty())
		{
			error_vertex_count = 0;
		}
		else
		{
			console_batcher->clear();
			// this requires the atlas texture to be bound with 1 byte packing
			if(!error_text.draw())
			{
				// put the message into the console instead
				post_error(serr_get_error());
			}
			error_vertex_count = console_batcher->get_current_vertex_count();
			if(console_batcher->get_quad_count() != 0)
			{
				ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_error_interleave_vbo);
				ctx.glBufferData(
					GL_ARRAY_BUFFER,
					console_batcher->get_total_vertex_size(),
					NULL,
					GL_STREAM_DRAW);
				ctx.glBufferSubData(
					GL_ARRAY_BUFFER,
					0,
					console_batcher->get_current_vertex_size(),
					console_batcher->buffer);
				ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}

	if(log_vertex_count != 0)
	{
		float x;
		float y;
		float w;
		float h;
		log_box.get_bbox(&x, &y, &w, &h);

		GLint scissor_x = x; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_y = y; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_w = w; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_h = h; // NOLINT(bugprone-narrowing-conversions)
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glBindVertexArray(gl_log_vao_id);
			ctx.glDrawArrays(GL_TRIANGLES, 0, log_vertex_count);
			ctx.glBindVertexArray(0);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
	}

	if(prompt_vertex_count != 0)
	{
		float x;
		float y;
		float w;
		float h;
		prompt_cmd.get_bbox(&x, &y, &w, &h);
		GLint scissor_x = x; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_y = y; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_w = w; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_h = h; // NOLINT(bugprone-narrowing-conversions)
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glBindVertexArray(gl_prompt_vao_id);
			ctx.glDrawArrays(GL_TRIANGLES, 0, prompt_vertex_count);
			ctx.glBindVertexArray(0);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
	}

	if(error_vertex_count != 0)
	{
		float x;
		float y;
		float w;
		float h;
		error_text.get_bbox(&x, &y, &w, &h);
		GLint scissor_x = x; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_y = y; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_w = w; // NOLINT(bugprone-narrowing-conversions)
		GLint scissor_h = h; // NOLINT(bugprone-narrowing-conversions)
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glBindVertexArray(gl_error_vao_id);
			ctx.glDrawArrays(GL_TRIANGLES, 0, error_vertex_count);
			ctx.glBindVertexArray(0);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
	}

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}

void console_state::post_error(std::string_view msg)
{
	error_text.replace_string(msg);
}
