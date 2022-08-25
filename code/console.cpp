#include "global.h"

#include "console.h"

#include "app.h"
#include "font/utf8_stuff.h"
#include "cvar.h"


#include <SDL2/SDL.h>

console_state g_console;

// this IS stupid, but a portable strtok is complicated.
// probably should put this into global.h if I used it more.
static char *musl_strtok_r(char *__restrict s, const char *__restrict sep, char **__restrict p)
{
    // NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if (!s && !(s = *p)) return NULL;
	s += strspn(s, sep);
    // NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if (!*s) return *p = 0;
	*p = s + strcspn(s, sep);
    // NOLINTNEXTLINE(readability-implicit-bool-conversion)
	if (**p) *(*p)++ = 0;
	else *p = 0;
	return s;
}

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
		   "", &log_batcher, TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY))
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

	if(!prompt_cmd.init("", &prompt_batcher, TEXTP_SINGLE_LINE | TEXTP_X_SCROLL | TEXTP_DRAW_BBOX))
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

	if(!error_text.init("", &error_batcher, TEXTP_READ_ONLY | TEXTP_DISABLE_CULL | TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

    // set to red.
    // TODO: it would be cool if the text background was black,
    // as if the text was being selected.
    error_text.text_color = {255,0,0,255};
    error_text.backdrop_color = RGBA8_PREMULT(0,0,0,255);

	//
    // finish
    //
	ctx.glBindVertexArray(0);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

    //set the area the text is placed.
    resize_text_area();

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

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

void console_state::resize_text_area()
{
	log_box.set_bbox(
		60,
		60,
		static_cast<float>(cv_screen_width.data) / 2,
		static_cast<float>(cv_screen_height.data) / 2);
	prompt_cmd.set_bbox(
		60,
		60 + static_cast<float>(cv_screen_height.data) / 2 + 10.f,
		static_cast<float>(cv_screen_width.data) / 2,
		prompt_batcher.GetLineSkip());

    // this probably doesn't have enough hieght to fit in messages with stack traces,
    // but if it's too big it could potentially block UI elements in an annoying way
	error_text.set_bbox(
		60,
		prompt_cmd.box_ymax + 10.f,
		static_cast<float>(cv_screen_width.data) / 2,
		prompt_batcher.GetLineSkip() * 10);
}

CONSOLE_RESULT console_state::input(SDL_Event& e)
{
	ASSERT(font_manager != NULL);

	switch(log_box.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::EAT:
		prompt_cmd.unfocus();
		error_text.unfocus();
		log_box.focus();
		return CONSOLE_RESULT::EAT;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	switch(prompt_cmd.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::EAT:
		error_text.unfocus();
		log_box.unfocus();
		prompt_cmd.focus();
		return CONSOLE_RESULT::EAT;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	// Only parse input when there is actual content.
	// possible because this is read only.
	if(error_batcher.vertex_count() != 0)
	{
		switch(error_text.input(e))
		{
		case TEXT_PROMPT_RESULT::CONTINUE: break;
		case TEXT_PROMPT_RESULT::EAT:
			prompt_cmd.unfocus();
			log_box.unfocus();
			error_text.focus();
			return CONSOLE_RESULT::EAT;
		case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
		}
	}

	switch(e.type)
	{
	case SDL_KEYUP:
		switch(e.key.keysym.sym)
		{
		case SDLK_RETURN:
			if(prompt_cmd.text_focus)
			{
                TIMER_U start;
                TIMER_U end;
                start = timer_now();

				parse_input();

                end = timer_now();
	            slogf("time: %f\n", timer_delta_ms(start, end));
			}
			break;
		}
		break;
	case SDL_KEYDOWN:
		switch(e.key.keysym.sym)
		{
		case SDLK_UP:
			// show old commands like a shell
			break;

		case SDLK_DOWN:
			// show newer commands like a shell
			break;
		}
		break;
	}

	return CONSOLE_RESULT::CONTINUE;
}

void console_state::parse_input()
{
	// TODO: this is way too simple, it can't support escape keys or quotes.
	// The quick and ugly solution would be to use boost spirit.
	std::string line = prompt_cmd.get_string();
	prompt_cmd.clear_string();
    
    // show the command in the log
    slogf("%s\n", line.c_str());

	std::vector<const char*> arguments;

	const char* delim = " ";
    char* next_token = NULL;
    char* token = musl_strtok_r(line.data(), delim, &next_token);
    while(token != NULL)
    {
        arguments.push_back(token);
        token = musl_strtok_r(NULL, delim, &next_token);
    }
    // NOLINTNEXTLINE(bugprone-narrowing-conversions)
    if(!cvar_args(CVAR_T::RUNTIME, arguments.size(), arguments.data()))
    {
        if(!error_text.replace_string(serr_get_error()))
        {
            // NOTE: what do I do here? 
        }
		// TODO: maybe if an error occurs during runtime, 
        // reserve a little text section at the top of the console
        // and store the most recent error in there,
        // then make the console appear (but without text focus)
	}
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
			}
		}
        
        log_box.set_readonly(false);
        // this requires the atlas texture to be bound with 1 byte packing
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        log_box.stb_insert_chars(log_box.text_data.size(), text_data, char_count);
        log_box.set_readonly(true);
		//log.text_data.insert(log.text_data.end(), text_data, text_data + char_count);

		// TODO: I don't always want this to scroll to the bottom,
		// I would like it to only do that when the scrollbar is already at the bottom!
		log_box.scroll_to_bottom();
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