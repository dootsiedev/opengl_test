#pragma once

#include "global.h"

#include "opengles2/opengl_stuff.h"
#include "shaders/pointsprite.h"
//#include "shaders/basic.h"
#include "shaders/mono.h"
#include "font/font_manager.h"
#include "font/text_prompt.h"
#include "console.h"
#include "options_menu/options_tree.h"
#include "cvar.h"

#include <SDL2/SDL.h>

#include <glm/vec2.hpp> // vec2
#include <glm/vec3.hpp> // vec3
#include <limits>

extern cvar_double cv_string_pt;
extern cvar_string cv_string_font;
extern cvar_double cv_string_outline;
extern cvar_int cv_string_mono;
extern cvar_int cv_string_force_bitmap;
extern cvar_double cv_string_alpha_test;

extern cvar_double cv_mouse_sensitivity;
extern cvar_double cv_camera_speed;
extern cvar_int cv_mouse_invert;

#include "keybind.h"
// keybinds
extern cvar_key_bind cv_bind_move_forward;
extern cvar_key_bind cv_bind_move_backward;
extern cvar_key_bind cv_bind_move_left;
extern cvar_key_bind cv_bind_move_right;
extern cvar_key_bind cv_bind_move_jump;
extern cvar_key_bind cv_bind_move_crouch;
extern cvar_key_bind cv_bind_fullscreen;
extern cvar_key_bind cv_bind_open_console;
extern cvar_key_bind cv_bind_open_options;
extern cvar_key_bind cv_bind_reset_window_size;
extern cvar_key_bind cv_bind_toggle_text;
extern cvar_key_bind cv_bind_soft_reboot;

// This is absolutely not the best way of doing this...
struct bench_data
{
	TIMER_RESULT accum = 0, high = 0, low = std::numeric_limits<TIMER_RESULT>::max();
	size_t samples = 0;
	void test(TIMER_RESULT dt)
	{
		low = std::min(low, dt);
		high = std::max(high, dt);
		accum += dt;
		++samples;
	}
	TIMER_RESULT accum_ms()
	{
		return accum / static_cast<TIMER_RESULT>(samples);
	}
	TIMER_RESULT high_ms()
	{
		return high;
	}
	TIMER_RESULT low_ms()
	{
		if(low == std::numeric_limits<TIMER_RESULT>::max())
		{
			return 0;
		}
		return low;
	}
	void reset()
	{
		high = 0; // could use limits...
		low = std::numeric_limits<TIMER_RESULT>::max();
		accum = 0;
		samples = 0;
	}

	NDSERR bool display(const char* msg, font_sprite_painter* font_painter);
};

enum class DEMO_RESULT
{
	CONTINUE,
	SOFT_REBOOT,
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

	NDSERR bool init_gl_point_sprite();
	NDSERR bool destroy_gl_point_sprite();

	shader_mono_state mono_shader;
	GLuint gl_font_interleave_vbo = 0;
	GLuint gl_font_vao_id = 0;
	GLsizei gl_font_vertex_count = 0;

	// GLuint gl_prompt_interleave_vbo = 0;
	// GLuint gl_prompt_vao_id = 0;

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
	// if you want to use unifont as a standalone style, use this.
	hex_font_placeholder unifont_style;

	// this puts the text on the screen using a style and batcher.
	font_sprite_painter font_painter;

	// convenience wrapper for drawing quads vertices into a buffer.
	// you only need one.
	mono_2d_batcher font_batcher;
	std::unique_ptr<gl_mono_vertex[]> font_batcher_buffer;

	bool show_text = true;

	options_tree_state option_menu;
	bool show_options = false;

	console_state console_menu;
	bool show_console = false;

	bool update_screen_resize = true;

	TIMER_U timer_last = TIMER_NULL;

	// this should be float or byte
	// but ATM i use this as a incrementing number
	double colors[3] = {};

	enum
	{
		MOVE_FORWARD,
		MOVE_BACKWARD,
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_JUMP,
		MOVE_CROUCH,
		MAX_MOVE
	};

	bool keys_down[MAX_MOVE] = {};
	float camera_yaw = 0;
	float camera_pitch = 0;
	glm::vec3 camera_pos = {};
	glm::vec3 camera_direction = {1.f, 0.f, 0.f};

	int point_buffer_size = 0;
	int ibo_buffer_size = 0;
	std::unique_ptr<GLushort[]> ibo_buffer;

	bench_data perf_total;
	bench_data perf_input;
	bench_data perf_update;
	bench_data perf_render;
#ifndef __EMSCRIPTEN__
	bench_data perf_swap;
#endif

	NDSERR bool init();
	NDSERR bool init_gl_font();

	NDSERR bool destroy();
	NDSERR bool destroy_gl_font();
	NDSERR bool update(double delta_sec);
	NDSERR bool input(SDL_Event& e);
	void unfocus_demo();
	bool unfocus_all();
	NDSERR bool render();

	NDSERR DEMO_RESULT process();

	NDSERR bool perf_time();
	NDSERR bool display_perf_text();
};
