#pragma once

#include "../ui.h"
#include "../font/text_prompt.h"
#include "options_cvar_template.h"

enum class OPTIONS_MENU_RESULT
{
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_list_state
{
	shared_cvar_option_state* shared_state = NULL;
	std::vector<std::unique_ptr<abstract_option_element>> option_entries;

	mono_y_scrollable_area scroll_state;

	// footer buttons
	std::string revert_text;
	std::string ok_text;
	std::string defaults_text;
	mono_button_object revert_button;
	mono_button_object ok_button;
	mono_button_object defaults_button;

	// since I only render when it is requested, I need to keep this.
	GLsizei menu_batch_vertex_count = 0;
	// the scroll goes right after the menu batch.
	GLsizei scroll_batch_vertex_count = 0;

	// added size to the lineskip for the button size.
	// float font_padding = 4;
	// padding between elements (buttons, scrollbar, etc)
	// float element_padding = 10;

	// the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	bool update_buffer = true;

	NDSERR bool init(shared_cvar_option_state* shared_state_);

	void resize_view();

	NDSERR OPTIONS_MENU_RESULT input(SDL_Event& e);

	// draw the backdrop and footer
	NDSERR bool draw_menu();

	// draw the scrollbar and contents.
	NDSERR bool draw_scroll();

	NDSERR bool update(double delta_sec);

	// this requires the atlas texture to be bound with 1 byte packing
	NDSERR bool render();

    NDSERR bool refresh();


	NDSERR bool undo_history();
	NDSERR bool clear_history();
	NDSERR bool set_defaults();
	NDSERR bool close();
};