#pragma once

#include "../cvar.h"
#include "../ui.h"
#include "../keybind.h"
#include "../font/text_prompt.h"

#include <SDL2/SDL.h>

#include <string>

enum class OPTION_ELEMENT_RESULT : uint8_t
{
	CONTINUE,
	// this is used to notify that the option is has history for undo_changes().
	// this does not need to be returned if there is already a modification (but maybe it's better)
	MODIFIED,
	ERROR
};

// this is a sub element of a option list.
struct abstract_option_element
{
	NDSERR virtual bool update(double delta_sec) = 0;
	NDSERR virtual OPTION_ELEMENT_RESULT input(SDL_Event& e) = 0;
	NDSERR virtual bool draw_buffer() = 0;
	virtual void resize(float x, float y, float menu_w) = 0;
	virtual bool draw_requested() = 0;
	// reset to default state (if there is any)
	// you don't need to clear the history, or treat this as set_hidden_event.
	// if any text is selected, unselect it, any scrolling, scroll to top.
	NDSERR virtual bool close() = 0;

	virtual float get_height() = 0;
	NDSERR virtual bool set_default() = 0;
	NDSERR virtual bool undo_changes() = 0;
	NDSERR virtual bool clear_history() = 0;
	// deal with the fact that cvars that changed (like from the console) wont be displayed
	// called before you open the menu.
	NDSERR virtual bool reload_cvars() = 0;

	virtual ~abstract_option_element() = default;
};

enum class FOCUS_ELEMENT_RESULT : uint8_t
{
	CONTINUE,
	// if returned, close() will be called for you.
	CLOSE,
	// same as OPTION_ELEMENT_RESULT::MODIFIED,
	// this is does CLOSE
	MODIFIED,
	ERROR
};

// focus element means an element that is drawn over the option menu.
// this could be a popup message or even a dropdown menu.
struct abstract_focus_element
{
	NDSERR virtual bool update(double delta_sec) = 0;
	NDSERR virtual FOCUS_ELEMENT_RESULT input(SDL_Event& e) = 0;
	NDSERR virtual bool draw_buffer() = 0;
	// you need to store the vertex offset from the batcher and make your own draw call.
	NDSERR virtual bool render() = 0;
	virtual bool draw_requested() = 0;
	virtual void resize_view() = 0;
	// reset to default state (if there is any)
	// you don't need to clear the history, or treat this as set_hidden_event.
	// if any text is selected, unselect it, any scrolling (not possible ATM) scroll to top.
	NDSERR virtual bool close() = 0;

	virtual ~abstract_focus_element() = default;
};

// recursive dependancy...
struct shared_cvar_option_state;

// this is an example of a focus element.
// a menu that shows message with a OK button
struct option_error_prompt : public abstract_focus_element
{
	shared_cvar_option_state* state = NULL;

	// needs a independant painter because I need newlines, and the global one shouldn't have it.
	// font_sprite_painter font_painter;

	// maybe if the message was a serr message,
	// I would use a prompt to allow selection, and a button for "copy to clipboard",
	// because serr is more of a programmer diagnostic and not comprehensible.
	// std::string display_message;
	text_prompt_wrapper prompt;

	std::string ok_button_text;
	mono_button_object ok_button;

	GLint gl_batch_buffer_offset = -1;
	GLsizei gl_batch_vertex_count = 0;
	GLsizei gl_batch_vertex_scroll_count = 0;

	float edge_padding = 100;

	// the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	NDSERR bool init(shared_cvar_option_state* state_, std::string_view message);

	void resize_view() override;

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR FOCUS_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	NDSERR bool render() override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		gl_batch_buffer_offset = 0;
		gl_batch_vertex_count = 0;
		return true;
	}
};

// forward declaration
struct cvar_keybind_option;

struct option_keybind_request : public abstract_focus_element
{
	shared_cvar_option_state* state = NULL;

	// needs a independant painter because I need newlines, and the global one shouldn't have it.
	font_sprite_painter font_painter;

	// the value before you press the OK button.
	keybind_state temp_value;
	bool value_modified = false;
	bool update_buffer = true;

	cvar_keybind_option* option_state = NULL;

	// maybe if the message was a serr message,
	// I would use a prompt to allow selection, and a button for "copy to clipboard",
	// because serr is more of a programmer diagnostic and not comprehensible.
	// std::string display_message;
	std::unique_ptr<char[]> display_message;
	int display_message_len = 0;

	std::string ok_button_text;
	mono_button_object ok_button;

	std::string cancel_button_text;
	mono_button_object cancel_button;

	std::string unbind_button_text;
	mono_button_object unbind_button;

	GLint gl_batch_buffer_offset = -1;
	GLsizei batch_vertex_count = 0;

	// the dimensions of the whole backdrop
	float box_xmin = -1;
	float box_xmax = -1;
	float box_ymin = -1;
	float box_ymax = -1;

	float text_width = -1;
	float text_height = -1;

	NDSERR bool init(shared_cvar_option_state* state_, cvar_keybind_option* option_state_);

	NDSERR bool format_text();
	void commit_change();

	void resize_view() override;

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR FOCUS_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	NDSERR bool render() override;
	bool draw_requested() override;
	NDSERR bool close() override;
};

// this needs to be allocated for the lifetime of the elements.
struct shared_cvar_option_state
{
	// if the element needs focus (be ontop of everything),
	// set the pointer to the thing (like a dropdown menu)
	// get_height, set_default, undo_changes, and clear_history are unused.
	abstract_focus_element* focus_element = NULL;
	font_sprite_painter* font_painter = NULL;

	// the error prompt for all the option elements.
	// this doesn't carry any state other than a message, so it's shared.
	// just call init() again.
	option_error_prompt error_prompt;
	option_keybind_request keybind_prompt;

	float font_padding = 4;
	float element_padding = 10;

	GLuint gl_options_interleave_vbo = 0;
	GLuint gl_options_vao_id = 0;

	void init(font_sprite_painter* font_painter_, GLuint vbo, GLuint vao)
	{
		ASSERT(font_painter_ != NULL);
		font_painter = font_painter_;
		gl_options_interleave_vbo = vbo;
		gl_options_vao_id = vao;
	}

	NDSERR bool set_focus(abstract_focus_element* element)
	{
		if(focus_element != NULL && !focus_element->close())
		{
			return false;
		}
		focus_element = element;
		return true;
	}
};

// simple on and off button
std::unique_ptr<abstract_option_element>
	create_bool_option(shared_cvar_option_state* state, std::string label, cvar_int* cvar);

// a dropdown menu for the options.
// TODO: create new cvars,
// like cvar_multi_option, and cvar_bool (inherited from cvar_int),
// so that you can rename/reorder/add/remove the options without
// needing to also update the option menu strings.
// also remember multi_option_entry should have the option to be disabled.
struct multi_option_entry
{
	// the value stored here will be MOVED out.
	std::string name;
	const char* cvar_value = NULL;
};

// note that this will use cvar_write and cvar_read for all modifications,
// which means "special" cvars like fullscreen and vsync can work.
std::unique_ptr<abstract_option_element> create_multi_option(
	shared_cvar_option_state* state,
	std::string label,
	V_cvar* cvar,
	size_t count,
	multi_option_entry* entries);

// a prompt with a string or whatever you want.
std::unique_ptr<abstract_option_element>
	create_prompt_option(shared_cvar_option_state* state, std::string label, V_cvar* cvar, bool long_prompt = false);

// a slider + prompt for a floating point number
// clamp will clamp numbers entered into the prompt.
// TODO: I feel like the cvar should have it's internal min/max,
// so I don't need "clamp", and the console should also give an error.
std::unique_ptr<abstract_option_element> create_slider_option(
	shared_cvar_option_state* state, std::string label, cvar_double* cvar, double min, double max);

std::unique_ptr<abstract_option_element>
	create_keybind_option(shared_cvar_option_state* state, std::string label, cvar_key_bind* cvar);