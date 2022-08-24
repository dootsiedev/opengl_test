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
    
    // create the buffer for the shader
    ctx.glGenBuffers(1, &gl_prompt_interleave_vbo);
	if(gl_prompt_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}
    // create the buffer for the shader
    ctx.glGenBuffers(1, &gl_log_interleave_vbo);
	if(gl_log_interleave_vbo == 0)
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

	// finish
	ctx.glBindVertexArray(0);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);


    // load the atlas texture.
    ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);

	prompt_batcher.SetFont(font_style);

	if(!prompt.init("", &prompt_batcher, TEXTP_SINGLE_LINE | TEXTP_X_SCROLL | TEXTP_DRAW_BBOX))
	{
		return false;
	}
	prompt.set_bbox(
		60,
		60 + static_cast<float>(cv_screen_height.data) / 2 + 10.f,
		static_cast<float>(cv_screen_width.data) / 2,
		prompt_batcher.GetLineSkip());

	log_batcher.SetFont(font_style);

	if(!log.init(
		   "", &log_batcher, TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY))
	{
		return false;
	}
	log.set_bbox(
		60,
		60,
		static_cast<float>(cv_screen_width.data) / 2,
		static_cast<float>(cv_screen_height.data) / 2);

	// restore to the default 4 alignment.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}
bool console_state::destroy()
{
    SAFE_GL_DELETE_VBO(gl_prompt_interleave_vbo);
    SAFE_GL_DELETE_VAO(gl_prompt_vao_id);
    SAFE_GL_DELETE_VBO(gl_log_interleave_vbo);
    SAFE_GL_DELETE_VAO(gl_log_vao_id);
	return GL_CHECK(__func__) == GL_NO_ERROR;
}

CONSOLE_RESULT console_state::input(SDL_Event& e)
{
    ASSERT(font_manager != NULL);
    
	// load the atlas texture because the prompt could load text from input
	// I don't like this, but this is generally not a very expensive operation.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);
	TEXT_PROMPT_RESULT prompt_ret = prompt.input(e);
	// restore to the default 4 alignment.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	switch(prompt_ret)
	{
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::EAT: log.unfocus(); return CONSOLE_RESULT::EAT;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);
	TEXT_PROMPT_RESULT log_ret = log.input(e);
	// restore to the default 4 alignment.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	switch(log_ret)
	{
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::EAT: prompt.unfocus(); return CONSOLE_RESULT::EAT;
	case TEXT_PROMPT_RESULT::ERROR: return CONSOLE_RESULT::ERROR;
	}

	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
        // TODO: shouldn't do this
		case SDL_WINDOWEVENT_RESIZED:
			log.set_bbox(
				60,
				60,
				static_cast<float>(e.window.data1) / 2,
				static_cast<float>(e.window.data2) / 2);
			prompt.set_bbox(
				60,
				60 + static_cast<float>(e.window.data2) / 2 + 10.f,
				static_cast<float>(e.window.data1) / 2,
				prompt_batcher.GetLineSkip());

			break;
        }
		break;
	case SDL_KEYUP:
		switch(e.key.keysym.sym)
		{
		case SDLK_RETURN:
			if(prompt.text_focus)
			{
                TIMER_U start;
                TIMER_U end;
                start = timer_now();

				parse_input();

				// note this will break undo / redo! (doesn't matter because read-only)
				// and assumes that log and prompt have the same font.
				log.text_data.insert(
					log.text_data.end(), prompt.text_data.begin(), prompt.text_data.end());

                
                // I insert a newline
                // If I used stb's paste, this would overwrite the selection.
				char32_t newl = '\n';
				log.set_readonly(false);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				log.stb_insert_chars(log.text_data.size(), &newl, 1);
				log.set_readonly(true);

				// tell the prompt to update itself (scroll_to_bottom implicitly sets this too)
				log.update_buffer = true;

				log.scroll_to_bottom();

				prompt.clear_string();

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
	std::string line = prompt.get_string();
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
        // leave it unhandled.
    }
}

bool console_state::draw()
{
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
		// load the atlas texture.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);
        
        log.set_readonly(false);
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        log.stb_insert_chars(log.text_data.size(), text_data, char_count);
        log.set_readonly(true);
		//log.text_data.insert(log.text_data.end(), text_data, text_data + char_count);

		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		// TODO: I don't always want this to scroll to the bottom,
		// I would like it to only do that when the scrollbar is already at the bottom!
		log.scroll_to_bottom();
	}
    if(!success)
    {
        return false;
    }
	ASSERT(font_manager != NULL);
    if(prompt.draw_requested())
	{
		// load the atlas texture.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);

		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_prompt_interleave_vbo);
		prompt_batcher.begin();
        //font_style.current_style = FONT_STYLE_OUTLINE;
        //prompt.text_color = {0,0,0,255};
		if(!prompt.draw())
        {
            return false;
        }
        #if 0
        font_style.current_style = FONT_STYLE_NORMAL;
        prompt.text_color = {255,255,255,255};
		if(!prompt.internal_draw_text(0, NULL, NULL, NULL))
        {
            return false;
        }
        #endif
		prompt_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}
    if(log.draw_requested())
	{
		// load the atlas texture.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager->gl_atlas_tex_id);

		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_log_interleave_vbo);
		log_batcher.begin();
        //font_style.current_style = FONT_STYLE_OUTLINE;
        //log.text_color = {0,0,0,255};
		if(!log.draw())
        {
            return false;
        }
        #if 0
        font_style.current_style = FONT_STYLE_NORMAL;
        prompt.text_color = {255,255,255,255};
		if(!prompt.internal_draw_text(0, NULL, NULL, NULL))
        {
            return false;
        }
        #endif
		log_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);

		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}
    return GL_RUNTIME(__func__) == GL_NO_ERROR;
}