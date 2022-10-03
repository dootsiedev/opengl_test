#pragma once

#include "../global.h"

#include "../opengles2/opengl_stuff.h"
#include "../shaders/mono.h"
#include "../font/font_manager.h"
#include "../ui.h"
#include "options_cvar_template.h"
#include "options_list.h"

#include <SDL2/SDL.h>

struct options_tree_state
{
	// this puts the text on the screen using a style and batcher.
	font_sprite_painter font_painter;

	shared_cvar_option_state shared_menu_state;

	struct menu_entry
	{
		std::string text;
		mono_button_object button;
		options_list_state menu_state;

		template<class T>
		std::vector<std::unique_ptr<abstract_option_element>>::reference add_option(T&& entry)
		{
			return menu_state.option_entries.emplace_back(std::forward<T>(entry));
		}

		bool good()
		{
			for(auto& entry : menu_state.option_entries)
			{
				if(!entry)
				{
					return false;
				}
			}
			return true;
		}
	};

	std::vector<menu_entry> menus;

	// -1 == show selection list.
	int current_menu_index = -1;

	std::string done_text;
	mono_button_object done_button;

	// the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	// the buffer that contains the menu rects and text
	// these are owned by this state.
	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

	NDSERR bool init(
		font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader);

	NDSERR bool destroy();

	NDSERR OPTIONS_MENU_RESULT input(SDL_Event& e);

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

	NDSERR OPTIONS_MENU_RESULT tree_input(SDL_Event& e);

	NDSERR bool tree_update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool tree_render();

	void tree_resize_view();
};