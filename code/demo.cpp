#include "global.h"

#include "demo.h"

#include "opengles2/opengl_stuff.h"

#include "RWops.h"
#include "font/font_manager.h"
#include "app.h"
#include "debug_tools.h"

#include <SDL2/SDL.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/mat4x4.hpp>

// Include GLM matrix extensions:
#include <glm/ext/matrix_float4x4.hpp> // mat4
#include <glm/ext/matrix_transform.hpp> // perspective, translate, rotate

#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <string>

static REGISTER_CVAR_STRING(cv_string, "test", "the string to display", CVAR_T::STARTUP);
static REGISTER_CVAR_DOUBLE(cv_string_pt, 16.0, "the point size of the string", CVAR_T::STARTUP);
static REGISTER_CVAR_STRING(
	cv_string_font, "seguiemj.ttf", "the font of the string", CVAR_T::STARTUP);
static REGISTER_CVAR_DOUBLE(
	cv_string_outline, 1, "outline thickness in pixels (if there is an outline)", CVAR_T::STARTUP);
static REGISTER_CVAR_INT(cv_string_mono, 0, "0 = off, 1 = use mono rasterizer", CVAR_T::STARTUP);
static REGISTER_CVAR_INT(
	cv_string_force_bitmap,
	0,
	"0 = off, 1 = on, can't bold or italics, but looks different",
	CVAR_T::STARTUP);

static REGISTER_CVAR_DOUBLE(
	cv_mouse_sensitivity, 0.4, "the speed of the first person camera", CVAR_T::RUNTIME);

struct gl_point_vertex
{
	GLfloat coords[3];
	GLubyte color[4];
	GLint inst_id;
};

/*
struct gl_font_vertex
{
	GLfloat pos[3];
	GLfloat tex[2];
	GLubyte color[4];
};*/

// creates a pattern like this with divisions = 1
//|\|\|
//|\|\|
// coordinates 0 to 1 origin topleft
// vbo must be have (divisions+2)^2 * 3 elements
// and ibo must have (divisions+1)^2 * 6 elements
static void create_grid(unsigned int divisions, glm::vec3* vbo_out, GLushort* ibo_out)
{
	unsigned int points = divisions + 2;
	unsigned int quads = divisions + 1;

	float fpoints = static_cast<float>(points);

	for(unsigned int i = 0, y = 0; y < points; ++y)
	{
		for(unsigned int x = 0; x < points; ++x)
		{
			float fx = static_cast<float>(x) / (fpoints - 1.f);
			float fy = static_cast<float>(y) / (fpoints - 1.f);
			float fz = fminf(sin(glm::radians(fx * 180.f)), sin(glm::radians(fy * 180.f))) * 0.05f;
			vbo_out[i++] = glm::vec3(fx, fy, fz);
		}
	}
	for(unsigned int i = 0, y = 0; y < quads; ++y)
	{
		for(unsigned int x = 0; x < quads; ++x)
		{
			ibo_out[i++] = y * points + x;
			ibo_out[i++] = y * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x + 1;
			ibo_out[i++] = (y + 1) * points + x;
			ibo_out[i++] = y * points + x;
		}
	}
}

// vbo must be have (divisions+2)^2 * 6 elements
// and ibo must have (divisions+1)^2 * 6 * 6 elements
static void create_grid_cube(unsigned int divisions, glm::vec3* vbo_out, GLushort* ibo_out)
{
	// note that this does not combine the vertexes of the edges due to laziness.
	// it's possible to just generate a ibo using just the vbo with a brute force algorithmn.
	unsigned int vbo_points = divisions + 2;
	unsigned int ibo_quads = divisions + 1;

	unsigned int side_vbo_points_size = vbo_points * vbo_points;
	unsigned int side_ibo_quads_size = ibo_quads * ibo_quads * 6;

	create_grid(divisions, vbo_out, ibo_out);

	// vbo, note the first side is already in vbo_out
	glm::mat4 orientations[6] = {
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f),
		glm::mat4(1.f)};
	// the first face will stay in the center since all other faces reference it.
	size_t face_cur = 0;
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(-0.5f, -0.5f, 0.f));
	// back
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// left
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(-90.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// right
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// top
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));
	// bottom
	++face_cur;
	orientations[face_cur] =
		glm::rotate(orientations[face_cur], glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
	orientations[face_cur] = glm::translate(orientations[face_cur], glm::vec3(0.f, 0.f, 0.5f));

	// orientations[1] = glm::rotate(orientations[1], glm::radians(90.f), glm::vec3(0.f,1.f,0.f));
	// orientations[1] = glm::translate(orientations[1], glm::vec3(0.f,0.f,1.f));

	// orientations[4] = glm::translate(orientations[4], glm::vec3(0.5f,0.5f,0.5f));

	size_t vbo_cur = 0;
	for(auto& orientation : orientations)
	{
		for(size_t i = 0; i < side_vbo_points_size; ++i)
		{
			vbo_out[vbo_cur++] = orientation * glm::vec4(vbo_out[i], 1.f);
		}
	}

	// move the first face to not be in the center
	orientations[0] = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, 0.5f));
	for(size_t i = 0; i < side_vbo_points_size; ++i)
	{
		vbo_out[i] = orientations[0] * glm::vec4(vbo_out[i], 1.f);
	}

	// ibo, note the first side is already in vbo_out
	for(size_t j = 1; j < 6; ++j)
	{
		for(size_t i = 0; i < side_ibo_quads_size; ++i)
		{
			ibo_out[j * side_ibo_quads_size + i] = ibo_out[i] + j * side_vbo_points_size;
		}
	}
}

bool demo_state::init()
{
	int max;
	ctx.glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);
	slogf("max texture: %d\n", max);

	ctx.glEnable(GL_CULL_FACE);
	ctx.glCullFace(GL_BACK);
	ctx.glFrontFace(GL_CCW);

	ctx.glEnable(GL_DEPTH_TEST);
	ctx.glDepthMask(GL_TRUE);

	// I forgot that glDrawElements doesn't work well with GL_LINES
	// because every vertex is processed once, so lines are missing.
	ctx.glLineWidth(4);

	// ctx.glEnable(GL_BLEND);
	// premultiplied
	ctx.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	// not premultiplied
	// ctx.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if(!mono_shader.create())
	{
		return false;
	}

	if(!point_shader.create())
	{
		return false;
	}

	if(!init_gl_point_sprite())
	{
		return false;
	}

	if(!init_gl_font())
	{
		return false;
	}

	timer_last = timer_now();
	return true;
}

bool demo_state::init_gl_point_sprite()
{
	SDL_Surface* test_image = SDL_LoadBMP("garfed.bmp");
	ASSERT(test_image != NULL);

	point_buffer_size = test_image->w * test_image->h;
	std::unique_ptr<gl_point_vertex[]> point_buffer(new gl_point_vertex[point_buffer_size]);

	ASSERT(test_image->format->BytesPerPixel == 3);

	ASSERT(test_image->format->format == SDL_PIXELFORMAT_BGR24);
	ASSERT(test_image->w * 3 == test_image->pitch);
	GLubyte* cur = static_cast<GLubyte*>(test_image->pixels);
	for(int y = 0; y < test_image->h; ++y)
	{
		for(int x = 0; x < test_image->w; ++x)
		{
			gl_point_vertex& point = point_buffer[y * test_image->w + x];
			point.coords[0] = static_cast<GLfloat>(x);
			point.coords[1] = static_cast<GLfloat>(y);
			point.coords[2] = 0;
			// note bgr
			point.color[2] = *cur++;
			point.color[1] = *cur++;
			point.color[0] = *cur++;
			point.color[3] = 255;
			point.inst_id = 0;
		}
	}
	SDL_FreeSurface(test_image);

	int divisions = 1;

	int points = divisions + 2;
	int vbo_size = points * points * 6;

	if(vbo_size > std::numeric_limits<GLushort>::max())
	{
		serrf("%s has too many vertexies: %d", __func__, vbo_size);
		return false;
	}

	std::unique_ptr<glm::vec3[]> vbo_buffer(new glm::vec3[vbo_size]);

	int quads = divisions + 1;
	ibo_buffer_size = quads * quads * 6 * 6;
	ibo_buffer.reset(new GLushort[ibo_buffer_size]);

	create_grid_cube(divisions, vbo_buffer.get(), ibo_buffer.get());

	for(int i = 0; i < vbo_size; ++i)
	{
		// I really should use math to figure this out but oh well.
		vbo_buffer[i] *= 1.424f;
	}

	// create the buffer for the shader
	GLuint gl_buffers[3];
	ctx.glGenBuffers(std::size(gl_buffers), gl_buffers);
	if(gl_buffers[0] == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}
	gl_vert_vbo_id = gl_buffers[0];
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_vert_vbo_id);
	ctx.glBufferData(
		GL_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		vbo_size * sizeof(decltype(vbo_buffer)::element_type),
		vbo_buffer.get(),
		GL_STATIC_DRAW);

	gl_vert_ibo_id = gl_buffers[1];
	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_vert_ibo_id);
	ctx.glBufferData(
		GL_ELEMENT_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		ibo_buffer_size * sizeof(decltype(ibo_buffer)::element_type),
		ibo_buffer.get(),
		GL_STATIC_DRAW);

	gl_point_vbo_id = gl_buffers[2];
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_point_vbo_id);
	ctx.glBufferData(
		GL_ARRAY_BUFFER,
		// NOLINTNEXTLINE(bugprone-narrowing-conversions)
		point_buffer_size * sizeof(gl_point_vertex),
		point_buffer.get(),
		GL_STATIC_DRAW);

	// basic VAO
	ctx.glGenVertexArrays(1, &gl_vao_id);
	if(gl_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}
	ctx.glBindVertexArray(gl_vao_id);

	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_vert_ibo_id);

	// position vertex setup
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_vert_vbo_id);
	if(point_shader.gl_attributes.a_vert_pos != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_vert_pos);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_vert_pos, // attribute
			3, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			0, // stride
			0 // array buffer offset
		);
	}

	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_point_vbo_id);
	if(point_shader.gl_attributes.a_point_pos != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_point_pos);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_point_pos, // attribute
			3, // size
			GL_FLOAT, // type
			GL_FALSE, // normalized?
			sizeof(gl_point_vertex), // stride
			(void*)offsetof(gl_point_vertex, coords) // NOLINT
		);
		ctx.glVertexAttribDivisor(point_shader.gl_attributes.a_point_pos, 1);
	}
	if(point_shader.gl_attributes.a_point_color != -1)
	{
		ctx.glEnableVertexAttribArray(point_shader.gl_attributes.a_point_color);
		ctx.glVertexAttribPointer(
			point_shader.gl_attributes.a_point_color, // attribute
			4, // size
			GL_UNSIGNED_BYTE, // type
			GL_TRUE, // normalized?
			sizeof(gl_point_vertex), // stride
			(void*)offsetof(gl_point_vertex, color) // NOLINT
		);
		ctx.glVertexAttribDivisor(point_shader.gl_attributes.a_point_color, 1);
	}

	// finish
	ctx.glBindVertexArray(0);

	ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
	ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// create the texture for the instance id matrix
	ctx.glGenTextures(1, &gl_inst_table_tex_id);
	if(gl_inst_table_tex_id == 0)
	{
		serrf("%s error: glGenTextures failed\n", __func__);
		return false;
	}

	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 1, 0, GL_RGBA, GL_FLOAT, NULL);
	// Set texture parameters
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// set uniform globals that aren't set every frame
	ctx.glUseProgram(point_shader.gl_program_id);
	ctx.glUniform1i(point_shader.gl_uniforms.u_inst_table, 0);
	ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glUseProgram(0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::destroy_gl_point_sprite()
{
	SAFE_GL_DELETE_VBO(gl_vert_vbo_id);
	SAFE_GL_DELETE_VBO(gl_vert_ibo_id);
	SAFE_GL_DELETE_VBO(gl_point_vbo_id);
	SAFE_GL_DELETE_VAO(gl_vao_id);
	SAFE_GL_DELETE_TEXTURE(gl_inst_table_tex_id);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::init_gl_font()
{
	if(!font_manager.create())
	{
		return false;
	}

	{
#if 1
		TIMER_U start;
		TIMER_U end;
		start = timer_now();
#endif
		Unique_RWops hex_file = Unique_RWops_OpenFS("unifont-full.hex", "rb");
		//Unique_RWops hex_file = Unique_RWops_OpenFS("unifont_upper-14.0.02.hex", "rb");
		if(!hex_file)
		{
			return false;
		}

		if(!font_manager.hex_font.init(std::move(hex_file)))
		{
			return false;
		}

#if 1
		// pretty fast for initializing every glyph in unicode.
		// 140ms on asan 24ms on reldeb.
		end = timer_now();
		slogf("time: %f\n", timer_delta_ms(start, end));

        // I used to load all the hex glyphs into memory
        // but it uses megabytes of memory...
        // now I lazy load.
		//size_t hex_state_size =
		//	font_manager.hex_font.hex_block_chunks.size() *
		//	sizeof(decltype(font_manager.hex_font.hex_block_chunks)::value_type);
		//slogf("hex memory used: %zu kb\n", hex_state_size / 1024);

#endif
	}

#if 1
	TIMER_U start;
	TIMER_U end;
	start = timer_now();
#endif

	//Unique_RWops test_font = Unique_RWops_OpenFS("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", "rb");
	Unique_RWops test_font = Unique_RWops_OpenFS(cv_string_font.data, "rb");
	if(!test_font)
	{
		return false;
	}

	if(!font_rasterizer.create(font_manager.FTLibrary, std::move(test_font)))
	{
		return false;
	}

	font_settings.point_size = static_cast<float>(cv_string_pt.data);

    if(cv_string_mono.data == 1)
    {
	    font_settings.render_mode = FT_RENDER_MODE_MONO;
	    font_settings.load_flags = FT_LOAD_TARGET_MONO;
    }

	// FT_LOAD_RENDER can give the same bitmap outline as force_bitmap
    // but force_bitmap will choose the closest raster bitmap possible,
    // so if a font supports both vector and raster data, and you want 
    // the text to be vectorized with a bitmap outline, use FT_LOAD_RENDER.
    // but this also breaks bold and italics style.
	//font_settings.load_flags = FT_LOAD_RENDER;
	//font_settings.load_flags = FT_LOAD_TARGET_MONO | FT_LOAD_RENDER;

	font_settings.bold_x = 1;
	font_settings.bold_y = 1;
	font_settings.italics_skew = 0.5;
	font_settings.outline_size = static_cast<float>(cv_string_outline.data);

    if(cv_string_force_bitmap.data == 1)
    {
        font_settings.force_bitmap = true;
    }


    font_rasterizer.set_face_settings(&font_settings);

	font_style.init(&font_manager, &font_rasterizer);
    




#if 1
	// it's pretty slow
	// 0.2ms on asan, 0.15 on reldeb
	// might need preloading + only load preloaded glyphs
	end = timer_now();
	slogf("time: %f\n", timer_delta_ms(start, end));
#endif

	// create the buffer for the shader
	ctx.glGenBuffers(1, &gl_font_interleave_vbo);
	if(gl_font_interleave_vbo == 0)
	{
		serrf("%s error: glGenBuffers failed\n", __func__);
		return false;
	}

	// basic VAO
	ctx.glGenVertexArrays(1, &gl_font_vao_id);
	if(gl_font_vao_id == 0)
	{
		serrf("%s error: glGenVertexArrays failed\n", __func__);
		return false;
	}

	// vertex setup
	ctx.glBindVertexArray(gl_font_vao_id);
	ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_font_interleave_vbo);
	gl_create_interleaved_mono_vertex_vao(mono_shader);



    // load the atlas texture.
    ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);
    bool ret = g_console.init(&font_style, mono_shader);
	// restore to the default 4 alignment.
	ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if(!ret)
    {
        return false;
    }

	// set uniform globals that aren't set every frame
	ctx.glUseProgram(mono_shader.gl_program_id);
	ctx.glUniform1i(mono_shader.gl_uniforms.u_tex, 0);

	glm::mat4 mvp = glm::ortho<float>(
		0,
		cv_screen_width.data, // NOLINT(bugprone-narrowing-conversions)
		cv_screen_height.data, // NOLINT(bugprone-narrowing-conversions)
		0,
		-1000,
		1000);
	ctx.glUniformMatrix4fv(mono_shader.gl_uniforms.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	ctx.glUseProgram(0);

	return GL_CHECK(__func__) == GL_NO_ERROR;
}

bool demo_state::destroy_gl_font()
{
	bool success = true;

    success = font_style.destroy() && success;
	success = font_rasterizer.destroy() && success;
	success = font_manager.destroy() && success;
	success = g_console.destroy() && success;

	SAFE_GL_DELETE_VBO(gl_font_interleave_vbo);
	SAFE_GL_DELETE_VAO(gl_font_vao_id);

	return GL_CHECK(__func__) == GL_NO_ERROR && success;
}

bool demo_state::destroy()
{
	bool success = true;

	success = destroy_gl_font() && success;
	success = destroy_gl_point_sprite() && success;

	success = point_shader.destroy() && success;
	success = mono_shader.destroy() && success;

	return success;
}

// return 0 for continue, 1 for exit.
DEMO_RESULT demo_state::input()
{
	SDL_Event e;
	while(SDL_PollEvent(&e) != 0)
	{
        //TIMER_U t1 = timer_now();

        
        if(show_console)
        {
            if(SDL_GetRelativeMouseMode() != SDL_TRUE)
            {
                ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);
                CONSOLE_RESULT ret = g_console.input(e);
                // restore to the default 4 alignment.
                ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                switch(ret)
                {
                case CONSOLE_RESULT::CONTINUE: break;
                case CONSOLE_RESULT::EAT: return DEMO_RESULT::CONTINUE;
                case CONSOLE_RESULT::ERROR: return DEMO_RESULT::ERROR;
                }
            } else {
                g_console.unfocus();
            }
        }
        
		switch(e.type)
		{
        case SDL_QUIT: return DEMO_RESULT::EXIT;
		case SDL_WINDOWEVENT:
			switch(e.window.event)
			{
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				cv_screen_width.data = e.window.data1;
				cv_screen_height.data = e.window.data2;
                g_console.resize_text_area();
				update_projection = true;
				break;
			}
			break;
		case SDL_KEYUP:
			// NOTE: probably should check this if I added in other keys
            // I wouldn't want to trigger if I was using a text prompt,
            // because key events will still be triggered even if textinput is active.
            //if(SDL_IsTextInputActive() == SDL_TRUE)
            
            switch(e.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				if(SDL_SetRelativeMouseMode(SDL_FALSE) < 0)
				{
					slogf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
				}
                if(show_console)
				{
					g_console.unfocus();
				}
				break;
            case SDLK_F1:
				if(show_console)
				{
					g_console.unfocus();
				    show_console = false;
				}
				else
				{
                    if(SDL_SetRelativeMouseMode(SDL_FALSE) < 0)
                    {
                        slogf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
                    }
					g_console.focus();
				    show_console = true;
				}
				break;
			case SDLK_F10:
                {
                    std::string msg;
                    msg += "StackTrace (f10):\n";
                    debug_str_stacktrace(&msg, 0);
                    msg += '\n';
                    slog_raw(msg.data(), msg.length());
                }
                break;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if(SDL_SetRelativeMouseMode(SDL_TRUE) < 0)
			{
				slogf("SDL_SetRelativeMouseMode failed: %s\n", SDL_GetError());
			}
			break;
		case SDL_MOUSEMOTION:
			if(SDL_GetRelativeMouseMode() == SDL_TRUE)
			{
				// int x;
				// int y;
				// SDL_GetRelativeMouseState(&x, &y);
				camera_yaw += static_cast<float>(e.motion.xrel * cv_mouse_sensitivity.data);
				camera_pitch -= static_cast<float>(e.motion.yrel * cv_mouse_sensitivity.data);
				camera_pitch = fmaxf(camera_pitch, -89.f);
				camera_pitch = fminf(camera_pitch, 89.f);

				glm::vec3 direction;
				direction.x = cos(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
				direction.y = sin(glm::radians(camera_pitch));
				direction.z = sin(glm::radians(camera_yaw)) * cos(glm::radians(camera_pitch));
				camera_direction = glm::normalize(direction);
			}
			break;
		}
        //another switch statement because I combine KEYDOWN and KEYUP
        switch(e.type)
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
            // key events will still be triggered even if textinput is active.
            // which means that prompts can't "EAT" the event because they don't use it.
            if(SDL_IsTextInputActive() == SDL_TRUE)
            {
                break;
            }
			switch(e.key.keysym.sym)
			{
			case SDLK_w: keys_down[MOVE_FORWARD] = (e.key.state == SDL_PRESSED); break;
			case SDLK_s: keys_down[MOVE_BACKWARD] = (e.key.state == SDL_PRESSED); break;
			case SDLK_a: keys_down[MOVE_LEFT] = (e.key.state == SDL_PRESSED); break;
			case SDLK_d: keys_down[MOVE_RIGHT] = (e.key.state == SDL_PRESSED); break;
			}
			break;
		}
	}
	return DEMO_RESULT::CONTINUE;
}
bool demo_state::render()
{
	TIMER_U current_time = timer_now();
	double color_delta = timer_delta<1>(timer_last, current_time);
	float float_delta = static_cast<float>(color_delta);
	timer_last = current_time;

    // this will not actually draw, this will just modify the atlas and buffer data.
	if(show_console)
    {
		// load the atlas texture.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);
        bool ret = g_console.draw();
		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        if(!ret)
        {
            return false;
        }
    }

	colors[0] = colors[0] + (0.5 * color_delta);
	colors[1] = colors[1] + (0.7 * color_delta);
	colors[2] = colors[2] + (0.11 * color_delta);

	const float cameraSpeed = 5.f * float_delta; // adjust accordingly
	glm::vec3 up = {0, 1, 0};

	if(keys_down[MOVE_FORWARD])
	{
		camera_pos += cameraSpeed * camera_direction;
	}

	if(keys_down[MOVE_BACKWARD])
	{
		camera_pos -= cameraSpeed * camera_direction;
	}

	if(keys_down[MOVE_LEFT])
	{
		camera_pos -= glm::normalize(glm::cross(camera_direction, up)) * cameraSpeed;
	}

	if(keys_down[MOVE_RIGHT])
	{
		camera_pos += glm::normalize(glm::cross(camera_direction, up)) * cameraSpeed;
	}

	ctx.glClearColor(
		static_cast<float>((sin(colors[0]) + 1.0) / 2.0),
		static_cast<float>((sin(colors[1]) + 1.0) / 2.0),
		static_cast<float>((sin(colors[2]) + 1.0) / 2.0),
		1.f);
	ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// identity atm;
	// glm::mat4x4 proj = glm::ortho<float>(0,50,0, 50, -1000, 1000);
	glm::mat4 proj = glm::perspective(
		glm::radians(80.0f),
		static_cast<float>(cv_screen_width.data) / static_cast<float>(cv_screen_height.data),
		0.2f,
		100.0f);

	glm::mat4 orientation = glm::rotate(
		glm::mat4(1.f),
		15.f,
		glm::vec3(
			static_cast<float>((sin(colors[0]) + 1.0) / 2.0),
			static_cast<float>((sin(colors[1]) + 1.0) / 2.0),
			static_cast<float>((sin(colors[2]) + 1.0) / 2.0)));

	glm::mat4x4 view = glm::lookAt(camera_pos, camera_pos + camera_direction, up);

	ctx.glDisable(GL_BLEND);
	ctx.glEnable(GL_DEPTH_TEST);

	ctx.glUseProgram(point_shader.gl_program_id);

	// This really should be a uniform buffer object, but it isnt supported in gl es 3.0 (needs 3.1)
	// it is also possible to just have an array of uniforms, but tbo's are bigger.
	// it's also possible to round robin the TBO if uploading is a bottleneck,
	// but for now I will just hope that glTexImage2D will not block like glTexSubImage2D
	// ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, gl_inst_table_tex_id);
	ctx.glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA32F, 1 * 4, 1, 0, GL_RGBA, GL_FLOAT, glm::value_ptr(orientation));

	ctx.glUniformMatrix4fv(point_shader.gl_uniforms.u_proj, 1, GL_FALSE, glm::value_ptr(proj));
	ctx.glUniformMatrix4fv(point_shader.gl_uniforms.u_view, 1, GL_FALSE, glm::value_ptr(view));

	ctx.glBindVertexArray(gl_vao_id);
	ctx.glDrawElementsInstanced(
		GL_TRIANGLES, ibo_buffer_size, GL_UNSIGNED_SHORT, NULL, point_buffer_size);
	ctx.glBindVertexArray(0);

	ctx.glEnable(GL_BLEND);
	ctx.glDisable(GL_DEPTH_TEST);
	ctx.glUseProgram(mono_shader.gl_program_id);

	if(update_projection)
	{
		update_projection = false;
		ctx.glViewport(0, 0, cv_screen_width.data, cv_screen_height.data);
		glm::mat4 mvp = glm::ortho<float>(
			0,
			cv_screen_width.data, // NOLINT(bugprone-narrowing-conversions)
			cv_screen_height.data, // NOLINT(bugprone-narrowing-conversions)
			0,
			-1000,
			1000);
		ctx.glUniformMatrix4fv(mono_shader.gl_uniforms.u_mvp, 1, GL_FALSE, glm::value_ptr(mvp));
	}

	// ctx.glActiveTexture(GL_TEXTURE0);
	ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);

	if(font_batcher.vertex_count() != 0)
    {
        ctx.glBindVertexArray(gl_font_vao_id);
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        ctx.glDrawArrays(GL_TRIANGLES, 0, font_batcher.vertex_count());
        ctx.glBindVertexArray(0);
    }
	if(show_console)
	{
		if(g_console.prompt_batcher.vertex_count() != 0)
		{
			float x;
			float y;
			float w;
			float h;
			g_console.prompt_cmd.get_bbox(&x, &y, &w, &h);
			GLint scissor_x = static_cast<GLint>(x);
			GLint scissor_y = static_cast<GLint>(y);
			GLint scissor_w = static_cast<GLint>(w);
			GLint scissor_h = static_cast<GLint>(h);
			if(scissor_w > 0 && scissor_h > 0)
			{
				ctx.glEnable(GL_SCISSOR_TEST);
				// don't forget that 0,0 is the bottom left corner...
				ctx.glScissor(
					scissor_x,
					cv_screen_height.data - (scissor_y + scissor_h),
					scissor_w,
					scissor_h);
				ctx.glBindVertexArray(g_console.gl_prompt_vao_id);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				ctx.glDrawArrays(GL_TRIANGLES, 0, g_console.prompt_batcher.vertex_count());
				ctx.glBindVertexArray(0);
				ctx.glDisable(GL_SCISSOR_TEST);
			}
		}
		if(g_console.log_batcher.vertex_count() != 0)
		{
			float x;
			float y;
			float w;
			float h;
			g_console.log_box.get_bbox(&x, &y, &w, &h);
			GLint scissor_x = static_cast<GLint>(x);
			GLint scissor_y = static_cast<GLint>(y);
			GLint scissor_w = static_cast<GLint>(w);
			GLint scissor_h = static_cast<GLint>(h);
			if(scissor_w > 0 && scissor_h > 0)
			{
				ctx.glEnable(GL_SCISSOR_TEST);
				// don't forget that 0,0 is the bottom left corner...
				ctx.glScissor(
					scissor_x,
					cv_screen_height.data - (scissor_y + scissor_h),
					scissor_w,
					scissor_h);
				ctx.glBindVertexArray(g_console.gl_log_vao_id);
				// NOLINTNEXTLINE(bugprone-narrowing-conversions)
				ctx.glDrawArrays(GL_TRIANGLES, 0, g_console.log_batcher.vertex_count());
				ctx.glBindVertexArray(0);
				ctx.glDisable(GL_SCISSOR_TEST);
			}
		}
        if(g_console.error_batcher.vertex_count() != 0)
		{
            ctx.glBindVertexArray(g_console.gl_error_vao_id);
            // NOLINTNEXTLINE(bugprone-narrowing-conversions)
            ctx.glDrawArrays(GL_TRIANGLES, 0, g_console.error_batcher.vertex_count());
            ctx.glBindVertexArray(0);
		}
	}
	ctx.glUseProgram(0);

	SDL_Delay(1);

	TIMER_U tick1;
	TIMER_U tick2;
	tick1 = timer_now();
	SDL_GL_SwapWindow(g_app.window);
	tick2 = timer_now();
	perf_swap.test(timer_delta_ms(tick1, tick2));

    return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
DEMO_RESULT demo_state::process()
{
	TIMER_U tick1;
	TIMER_U tick2;

	tick1 = timer_now();
	DEMO_RESULT ret = input();
    if(ret != DEMO_RESULT::CONTINUE) return ret;
	tick2 = timer_now();
	perf_input.test(timer_delta_ms(tick1, tick2));

	tick1 = tick2;
	if(!render())
    {
        return DEMO_RESULT::ERROR;
    }
	tick2 = timer_now();
	perf_render.test(timer_delta_ms(tick1, tick2));

	return perf_time() ? DEMO_RESULT::CONTINUE : DEMO_RESULT::ERROR;
}

bool demo_state::perf_time()
{
	TIMER_U tick_now = timer_now();

	// NOTE: "total" will include the time of displaying the data, but render wont.
	// but the time to display the data takes more time that I thought... (~1ms high peak)
	static TIMER_U total_start = tick_now;
	// static bench_data total_data;
	perf_total.test(timer_delta_ms(total_start, tick_now));
	total_start = tick_now;

	static TIMER_U display_timer = tick_now;

	if(timer_delta_ms(display_timer, tick_now) > 100)
	{
		bool success = true;
		display_timer = tick_now;

		// since the text is stored in a GL_RED texture,
		// I would need to pad each row to align to 4, but I don't.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		// load the atlas texture.
		ctx.glBindTexture(GL_TEXTURE_2D, font_manager.gl_atlas_tex_id);
		ctx.glBindBuffer(GL_ARRAY_BUFFER, gl_font_interleave_vbo);


        font_batcher.begin();


        font_batcher.SetFont(&font_style);

        //font_settings.render_mode = FT_RENDER_MODE_NORMAL;

		font_style.set_style(FONT_STYLE_OUTLINE);
		font_batcher.set_color(0, 0, 0, 255);
		success = success && display_perf_text();

        //font_settings.render_mode = FT_RENDER_MODE_MONO;

		font_style.set_style(FONT_STYLE_NORMAL);
		font_batcher.set_color(255, 255, 255, 255);
		success = success && display_perf_text();


		font_batcher.end();
		ctx.glBindBuffer(GL_ARRAY_BUFFER, 0);
		// restore to the default 4 alignment.
		ctx.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		success = success && GL_RUNTIME(__func__) == GL_NO_ERROR;

		if(!success)
		{
			return false;
		}

		perf_total.reset();
		perf_input.reset();
		perf_render.reset();
		perf_swap.reset();

        static bool first_sample = false;
        if(first_sample == false)
        {
            first_sample = true;
            TIMER_U first_sample_time = timer_now();
            slogf("first_text_pass: %f\n", timer_delta_ms(tick_now, first_sample_time));
        }
	}
	return true;
}

bool demo_state::display_perf_text()
{
	bool success = true;

	font_batcher.SetFlag(TEXT_FLAGS::NEWLINE);

	float x = 2;
	float y = 2;
#if 0
    font_batcher.SetXY(x, y);

	font_batcher.SetAnchor(TEXT_ANCHOR::TOP_LEFT);

	//font_batcher.Limit(100);
	success = success && font_batcher.draw_text(cv_string.data.c_str(), cv_string.data.size());
	// font_batcher.Newline();
#endif
	x = static_cast<float>(cv_screen_width.data) - 2;
	y = 2;
	font_batcher.SetXY(x, y);
	font_batcher.SetAnchor(TEXT_ANCHOR::TOP_RIGHT);

	//font_batcher.Limit(50);
	success = success && font_batcher.draw_text("average / low / high\n");
	//font_batcher.Limit(50);
	success = success && perf_total.display("total", &font_batcher);
	//font_batcher.Limit(50);
	success = success && perf_input.display("input", &font_batcher);
	//font_batcher.Limit(50);
	success = success && perf_render.display("render", &font_batcher);
	//font_batcher.Limit(50);
    success = success && perf_swap.display("swap", &font_batcher);
	return success;
}

bool bench_data::display(
	const char* msg, font_sprite_batcher* font_batcher)
{
	return font_batcher->draw_format("%s: %.2f / %.2f / %.2f\n", msg, accum_ms(), low_ms(), high_ms());
}