#pragma once

#include "global.h"

#include "opengles2/opengl_stuff.h"
#include "shaders/pointsprite.h"
//#include "shaders/basic.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"
#include "console.h"

#include <glm/vec2.hpp>           // vec2
#include <glm/vec3.hpp>           // vec3
#include <limits>

//TODO: This is absolutely not the best way of doing this...
struct bench_data
{
    //TODO: numeric_limits doesn't work if TIMER_U is a chrono time_point.
    TIMER_U accum = 0, high = 0, low = std::numeric_limits<TIMER_U>::max();
    size_t samples = 0;
    void test(TIMER_U dt)
    {
        if(dt < low) low = dt;
        if(dt > high) high = dt;
        accum += dt;
        ++samples;
    }
    TIMER_RESULT accum_ms()
    {
        return timer_delta_ms(0, accum) / static_cast<TIMER_RESULT>(samples);
    }
    TIMER_RESULT high_ms()
    {
        return timer_delta_ms(0, high);
    }
    TIMER_RESULT low_ms()
    {
        //this triggers UBsan if you tried to check low_ms without a single sample
        if(low == std::numeric_limits<TIMER_U>::max())
        {
            return 0;
        }
        return timer_delta_ms(0, low);
    }
    void reset()
    {
        high = 0; // could use limits...
        low = std::numeric_limits<TIMER_U>::max();
        accum = 0;
        samples = 0;
    }

    bool display(const char* msg, font_sprite_batcher* font_batcher);
};

enum class DEMO_RESULT
{
    CONTINUE,
    EXIT,
    ERROR
};

struct demo_state
{
	shader_pointsprite_state point_shader;

    GLuint gl_inst_table_tex_id = 0;
	GLuint gl_vert_vbo_id = 0;
	GLuint gl_vert_ibo_id = 0;
	GLuint gl_point_vbo_id = 0;
	GLuint gl_vao_id = 0;

    bool init_gl_point_sprite();
    bool destroy_gl_point_sprite();

	shader_mono_state mono_shader;
    GLuint gl_font_interleave_vbo = 0;
	GLuint gl_font_vao_id = 0;

    //GLuint gl_prompt_interleave_vbo = 0;
    //GLuint gl_prompt_vao_id = 0;

    // stores the atlas and fallback hexfont
	font_manager_state font_manager;
    // stores the settings for the font_rasterizer
    // this should only be initialized and not modified in runtime,
    // but if you want you can modify the settings between styles,
    // like FONT_STYLE_OUTLINE using a different FT_RENDER_MODE
    // if you are very careful.
    font_ttf_face_settings font_settings;
    // owns the TTF file, and rasters bitmaps.
    // you should only have one rasterizer for a font,
    // and switch styles with font_ttf_face_settings.
	font_ttf_rasterizer font_rasterizer;
    // places glyphs into the atlas texture, and
    // stores the location of glyphs in the atlas
	font_bitmap_cache font_style;
    // writes verticies for opengl, can be used with multiple fonts.
	font_sprite_batcher font_batcher;
    //prompt?
	//font_sprite_batcher prompt_batcher;
    //text_prompt_wrapper prompt;
    //console_state console; 
    bool show_console = false;

    bool update_projection = false;

	TIMER_U timer_last = 0;

	//this should be float or byte
    //but ATM i use this as a incrementing number
    double colors[3]= {};

	enum{
		MOVE_FORWARD,
		MOVE_BACKWARD,
		MOVE_LEFT,
		MOVE_RIGHT,
		MAX_MOVE
	};

	bool keys_down[MAX_MOVE] = {};
	float camera_yaw = 0;
	float camera_pitch = 0;
	glm::vec3 camera_pos = {};
	glm::vec3 camera_direction = {1.f,0.f,0.f};


	int point_buffer_size = 0;
	int ibo_buffer_size = 0;
	std::unique_ptr<GLushort[]> ibo_buffer;

    bench_data perf_total;
    bench_data perf_input;
    bench_data perf_render;
    bench_data perf_swap;

    bool init();
	bool init_gl_font();

    bool destroy();
    bool destroy_gl_font();

    DEMO_RESULT input();
    bool render();

    DEMO_RESULT process();

    bool perf_time();
    bool display_perf_text();
};