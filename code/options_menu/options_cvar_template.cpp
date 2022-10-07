#include "../global_pch.h"
#include "../global.h"

#include "options_cvar_template.h"

#include "../ui.h"
#include "../font/text_prompt.h"
#include "../font/utf8_stuff.h"
#include "../app.h"

bool option_error_prompt::init(shared_cvar_option_state* state_, std::string_view message)
{
	ASSERT(state_ != NULL);

	state = state_;

	ok_button_text = "ok";
	ok_button.init(state->font_painter);

	prompt.state.font_scale = state->font_painter->state.font_scale;

	if(!prompt.init(
		   message,
		   state->font_painter->state.batcher,
		   state->font_painter->state.font,
		   TEXTP_Y_SCROLL | TEXTP_WORD_WRAP | TEXTP_DRAW_BBOX | TEXTP_READ_ONLY |
			   TEXTP_DRAW_BACKDROP))
	{
		return false;
	}

	prompt.backdrop_color = RGBA8_PREMULT(255, 0, 0, 200);
	prompt.text_color = {255, 255, 255, 255};

	resize_view();

	return true;
}

void option_error_prompt::resize_view()
{
	ASSERT(state != NULL);

	font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	// 200px wide for 16px
	float prompt_width = std::max(screen_width / 2, 200 * (font_painter->get_lineskip() / 16.f));
	float prompt_height = std::max(screen_height / 2, font_painter->get_lineskip() * 20);

	// for a 16px font I would want 60px
	float button_width = 60 * (font_painter->get_lineskip() / 16.f);
	float button_height = font_painter->get_lineskip() + font_padding;

	float footer_height = button_height;
	// float footer_width = button_width;

	float max_menu_width = screen_width - edge_padding * 2;
	float max_menu_height = screen_height - edge_padding * 2;

	float menu_width = std::min(prompt_width, max_menu_width);
	float menu_height = std::min(prompt_height + element_padding + footer_height, max_menu_height);

	float x = std::floor((screen_width - menu_width) / 2.f);
	float y = std::floor((screen_height - menu_height) / 2.f);

	prompt.set_bbox(x, y, menu_width, menu_height - (footer_height + element_padding));

	// footer buttons
	{
		float x_cursor = x + menu_width;
		x_cursor -= button_width; //+ element_padding;
		ok_button.set_rect(x_cursor, y + menu_height - footer_height, button_width, button_height);
	}

	box_xmin = x - element_padding;
	box_xmax = x + menu_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + menu_height + element_padding;
}

bool option_error_prompt::update(double delta_sec)
{
	ASSERT(state != NULL);
	ok_button.update(delta_sec);
	prompt.update(delta_sec);
	return true;
}
FOCUS_ELEMENT_RESULT option_error_prompt::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	switch(ok_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER: return FOCUS_ELEMENT_RESULT::CLOSE;
	case BUTTON_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
	}

	if(e.type == SDL_KEYDOWN)
	{
		switch(e.key.keysym.sym)
		{
		case SDLK_ESCAPE:
		case SDLK_RETURN: return FOCUS_ELEMENT_RESULT::CLOSE;
		}
	}

	switch(prompt.input(e))
	{
	case TEXT_PROMPT_RESULT::CONTINUE:
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::UNFOCUS: break;
	case TEXT_PROMPT_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
	}

	// backdrop
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		// helps unfocus other elements.
		if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x && box_xmin <= mouse_x)
		{
			// eat
			set_event_leave(e);
			return FOCUS_ELEMENT_RESULT::CONTINUE;
		}
	}
	break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			// helps unfocus other elements.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				// eat
				set_event_unfocus(e);
				return FOCUS_ELEMENT_RESULT::CONTINUE;
			}
		}
		break;
	}

	return FOCUS_ELEMENT_RESULT::CONTINUE;
}
bool option_error_prompt::draw_buffer()
{
	ASSERT(state != NULL);

	font_sprite_painter* font_painter = state->font_painter;
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	gl_batch_buffer_offset = batcher->get_current_vertex_count();

	resize_view();

	// backdrop
	{
		float xmin = box_xmin;
		float xmax = box_xmax;
		float ymin = box_ymin;
		float ymax = box_ymax;

		std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}

	if(!ok_button.draw_buffer(ok_button_text.c_str(), ok_button_text.size()))
	{
		// NOLINTNEXTLINE
		return false;
	}

	gl_batch_vertex_count = batcher->get_current_vertex_count();

	if(!prompt.draw())
	{
		return false;
	}
	gl_batch_vertex_scroll_count = batcher->get_current_vertex_count();

	return true;
}
bool option_error_prompt::render()
{
	ASSERT(state != NULL);
	ASSERT(gl_batch_buffer_offset != -1);

	GLint vertex_offset = gl_batch_buffer_offset;
	GLsizei vertex_count = gl_batch_vertex_count;

	if(vertex_count - vertex_offset > 0)
	{
		vertex_count -= vertex_offset;

		// optimization: some render() calls use a draw call, and some dont,
		// This is a microoptimization because focus_element isn't a hot path (only called once)
		// but maybe I might want options to be more complex and elements could have multiple draw
		// calls. maybe the render() call should turn into render(GLint *offset, GLsizei *count), if
		// you don't need a exclusive draw call, just append *count. if you do, then you must draw
		// the previous batch (because it's likely you need a exclusive draw call because you are
		// using glScissor), then use your exclusive draw. and then after drawing do *offset +=
		// *count; *count = 0; then when all the elements are draw, make sure to complete the last
		// draw call.
		ctx.glDrawArrays(GL_TRIANGLES, vertex_offset, vertex_count);
		vertex_offset += vertex_count;
	}
	vertex_count = gl_batch_vertex_scroll_count;
	if(vertex_count - vertex_offset > 0)
	{
		vertex_count -= vertex_offset;

		// the scroll box
		GLint scissor_x = static_cast<GLint>(prompt.box_xmin);
		GLint scissor_y = static_cast<GLint>(prompt.box_ymin);
		GLint scissor_w = static_cast<GLint>(prompt.box_xmax - prompt.box_xmin);
		GLint scissor_h = static_cast<GLint>(prompt.box_ymax - prompt.box_ymin);
		if(scissor_w > 0 && scissor_h > 0)
		{
			ctx.glEnable(GL_SCISSOR_TEST);
			// don't forget that 0,0 is the bottom left corner...
			ctx.glScissor(
				scissor_x, cv_screen_height.data - scissor_y - scissor_h, scissor_w, scissor_h);
			ctx.glDrawArrays(GL_TRIANGLES, vertex_offset, vertex_count);
			ctx.glDisable(GL_SCISSOR_TEST);
		}
	}

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
bool option_error_prompt::draw_requested()
{
	ASSERT(state != NULL);

	return ok_button.draw_requested() || prompt.draw_requested();
}

// this is mainly for on or off buttons, but you can have more than 2 states to cycle.
struct cvar_button_multi_option : public abstract_option_element
{
	shared_cvar_option_state* state = NULL;
	V_cvar* cvar = NULL;
	std::string label_text;
	float element_height = -1;
	std::string previous_cvar_value;
	bool value_changed = false;

	std::unique_ptr<multi_option_entry[]> option_entries;
	multi_option_entry error_entry;
	int option_entries_size = -1;

	// if -1 use error_entry
	int current_button_index = -1;

	float font_x = -1;
	float font_y = -1;

	mono_button_object button;

	NDSERR bool init(
		shared_cvar_option_state* state_,
		std::string label,
		V_cvar* cvar_,
		size_t count,
		multi_option_entry* entries);

	// sets current_button_index
	NDSERR bool find_current_index();

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	void resize(float x, float y, float menu_w) override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		return true;
	};

	float get_height() override;
	NDSERR bool set_default() override;
	NDSERR bool undo_changes() override;
	NDSERR bool clear_history() override;
	NDSERR bool reload_cvars() override;
};

bool cvar_button_multi_option::init(
	shared_cvar_option_state* state_,
	std::string label,
	V_cvar* cvar_,
	size_t count,
	multi_option_entry* entries)
{
	ASSERT(state_ != NULL);
	ASSERT(cvar_ != NULL);
	ASSERT(count > 0);
	ASSERT(entries != NULL);

	state = state_;
	label_text = std::move(label);
	cvar = cvar_;

	button.init(state->font_painter);

	element_height = state->font_painter->get_lineskip() + state->font_padding;

	option_entries = std::make_unique<decltype(option_entries)::element_type[]>(count);
	// NOLINTNEXTLINE(bugprone-narrowing-conversions)
	option_entries_size = count;
	for(int i = 0; i < option_entries_size; ++i)
	{
		option_entries[i] = std::move(entries[i]);
	}

	return find_current_index();
}

bool cvar_button_multi_option::find_current_index()
{
	int index = -1;

	std::string current_cvar_value = cvar->cvar_write();

	for(int i = 0; i < option_entries_size; ++i)
	{
		if(current_cvar_value == option_entries[i].cvar_value)
		{
			index = i;
		}
	}
	if(index == -1)
	{
		error_entry.name = std::move(current_cvar_value);
		slogf(
			"info: couldn't find a button label for the cvar: `+%s` (%s).\n",
			cvar->cvar_key,
			error_entry.name.c_str());
		current_button_index = -1;
		// TODO: could be a serr error, but there could be undocumented values that shouldn't
		// trigger serr.
		return true;
	}
	current_button_index = index;

	return true;
}

bool cvar_button_multi_option::update(double delta_sec)
{
	ASSERT(state != NULL);
	button.update(delta_sec);
	return true;
}
OPTION_ELEMENT_RESULT cvar_button_multi_option::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	switch(button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		if(!value_changed)
		{
			previous_cvar_value = cvar->cvar_write();
			value_changed = true;

			if(cvar->cvar_type != CVAR_T::RUNTIME)
			{
				switch(cvar->cvar_type)
				{
				case CVAR_T::STARTUP:
					slogf(
						"info: +%s: this value is used in startup, which means that this change requires a restart take effect.\n",
						cvar->cvar_key);
					break;
				case CVAR_T::DEFFERRED:
					slogf(
						"info +%s: this value is deferred, which means that this change will not make immediately take effect.\n",
						cvar->cvar_key);
					break;
				default: slogf("info +%s: I don't know why this is here\n", cvar->cvar_key); break;
				}
			}
		}
		// needs to be done because it's possible the value changed.
		if(!find_current_index())
		{
			return OPTION_ELEMENT_RESULT::ERROR;
		}
		if(current_button_index == -1)
		{
			current_button_index = 0;
		}
		else
		{
			current_button_index = (current_button_index + 1) % option_entries_size;
		}
		if(!cvar->cvar_read(option_entries[current_button_index].cvar_value))
		{
			return OPTION_ELEMENT_RESULT::ERROR;
		}
		return OPTION_ELEMENT_RESULT::MODIFIED;
	case BUTTON_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	}
	return OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_button_multi_option::draw_buffer()
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	// the description text
	{
		font_painter->begin();
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(0, 0, 0, 255);
		font_painter->set_xy(font_x, font_y);
		font_painter->set_anchor(TEXT_ANCHOR::TOP_LEFT);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->set_style(FONT_STYLE_NORMAL);
		font_painter->set_color(255, 255, 255, 255);
		font_painter->set_xy(font_x, font_y);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->end();
	}
	const char* text;
	size_t text_size;
	if(current_button_index == -1)
	{
		text = error_entry.name.c_str();
		text_size = error_entry.name.size();
	}
	else
	{
		ASSERT(current_button_index < option_entries_size);
		ASSERT(current_button_index >= 0);
		text = option_entries[current_button_index].name.c_str();
		text_size = option_entries[current_button_index].name.size();
	}
	return button.draw_buffer(text, text_size);
}

void cvar_button_multi_option::resize(float x, float y, float menu_w)
{
	ASSERT(state != NULL);
	// font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	font_x = x + font_padding / 2.f;
	font_y = y + font_padding / 2.f;

	float cur_x = x + (menu_w - element_padding) / 2 + element_padding;
	button.set_rect(cur_x, y, (x + menu_w) - cur_x, element_height);
}

bool cvar_button_multi_option::draw_requested()
{
	return button.draw_requested();
}

float cvar_button_multi_option::get_height()
{
	return element_height;
}

bool cvar_button_multi_option::set_default()
{
	ASSERT(state != NULL);
	if(!value_changed)
	{
		previous_cvar_value = cvar->cvar_write();
		value_changed = true;
	}
	return find_current_index();
}
bool cvar_button_multi_option::undo_changes()
{
	ASSERT(state != NULL);

	if(value_changed)
	{
		if(!cvar->cvar_read(previous_cvar_value.c_str()))
		{
			return false;
		}
		previous_cvar_value.clear();
		value_changed = false;
	}
	return find_current_index();
}
bool cvar_button_multi_option::clear_history()
{
	ASSERT(state != NULL);
	previous_cvar_value.clear();
	value_changed = false;
	return true;
}
bool cvar_button_multi_option::reload_cvars()
{
	ASSERT(state != NULL);
	return find_current_index();
}

std::unique_ptr<abstract_option_element>
	create_bool_option(shared_cvar_option_state* state, std::string label, cvar_int* cvar)
{
	multi_option_entry bool_options[] = {{"off", "0"}, {"on", "1"}};
	auto output = std::make_unique<cvar_button_multi_option>();
	if(!output->init(state, std::move(label), cvar, std::size(bool_options), bool_options))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

std::unique_ptr<abstract_option_element> create_multi_option(
	shared_cvar_option_state* state,
	std::string label,
	V_cvar* cvar,
	size_t count,
	multi_option_entry* entries)
{
	auto output = std::make_unique<cvar_button_multi_option>();
	if(!output->init(state, std::move(label), cvar, count, entries))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

struct cvar_slider_option : public abstract_option_element
{
	shared_cvar_option_state* state = NULL;
	std::string label_text;
	cvar_double* cvar = NULL;

	mono_normalized_slider_object slider;
	text_prompt_wrapper prompt;

	double previous_value = NAN;
	double min_value = NAN;
	double max_value = NAN;

	float element_height = -1;

	float font_x = -1;
	float font_y = -1;

	NDSERR bool init(
		shared_cvar_option_state* state_,
		std::string label,
		cvar_double* cvar_,
		double min,
		double max);

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	void resize(float x, float y, float menu_w) override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		// the prompt and slider will unfocus
		// because set_event_hidden will be triggered.
		// but the prompt won't remove the selection so I need to do that here.
		prompt.clear_selection();
		return true;
	};

	float get_height() override;
	NDSERR bool set_default() override;
	NDSERR bool undo_changes() override;
	NDSERR bool clear_history() override;
	NDSERR bool reload_cvars() override;
};

bool cvar_slider_option::init(
	shared_cvar_option_state* state_, std::string label, cvar_double* cvar_, double min, double max)
{
	ASSERT(state_ != NULL);
	ASSERT(cvar_ != NULL);

	state = state_;
	label_text = std::move(label);
	cvar = cvar_;

	min_value = min;
	max_value = max;

	font_sprite_painter* font_painter = state->font_painter;

	element_height = font_painter->get_lineskip() + state->font_padding;

	slider.init(font_painter, cvar->data, min, max);

	// note this only works if you do it before init
	prompt.state.font_scale = font_painter->state.font_scale;

	// TODO(dootsie): I would make the prompt select all the text when you click up without
	// dragging. or just implement double / triple clicking...
	if(!prompt.init(
		   std::to_string(cvar->data),
		   font_painter->state.batcher,
		   font_painter->state.font,
		   TEXTP_SINGLE_LINE | TEXTP_DRAW_BBOX | TEXTP_DRAW_BACKDROP | TEXTP_DISABLE_CULL))
	{
		// NOLINTNEXTLINE
		return false;
	}

	return true;
}

bool cvar_slider_option::update(double delta_sec)
{
	ASSERT(state != NULL);
	// slider.update(delta_sec);
	prompt.update(delta_sec);
	return true;
}
OPTION_ELEMENT_RESULT cvar_slider_option::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	bool modified = false;
	if(slider.input(e))
	{
		std::string number_text = std::to_string(slider.get_value());
		if(!cvar->cvar_read(number_text.c_str()))
		{
			if(!state->error_prompt.init(state, serr_get_error()))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
			if(!state->set_focus(&state->error_prompt))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
		}
		else
		{
			modified = true;
			if(isnan(previous_value))
			{
				previous_value = cvar->data;
				// I would only do this if I had a unfocus event
#if 0
            // TODO: this is copy pasted
			if(cvar->cvar_type != CVAR_T::RUNTIME)
			{
				switch(cvar->cvar_type)
				{
				case CVAR_T::STARTUP:
					slogf(
						"info: +%s: this value is used in startup, which means that this change requires a restart take effect.\n",
						cvar->cvar_key);
					break;
				case CVAR_T::DEFFERRED:
					slogf(
						"info +%s: this value is deferred, which means that this change will not make immediately take effect.\n",
						cvar->cvar_key);
					break;
				default: slogf("info +%s: I don't know why this is here\n", cvar->cvar_key); break;
				}
			}
#endif
			}
			prompt.replace_string(number_text.c_str());
		}
	}

	bool parse_event = false;
	if(prompt.text_focus)
	{
		if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
		{
			parse_event = true;
			// I forgot why this is here
			slider.unfocus();
			// eat
			set_event_unfocus(e);
		}
	}
	switch(prompt.input(e))
	{
	// ignore modified because it might be annoying to have an error on every modification.
	// BUT I could make the text change to a red color if the input has an error.
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	case TEXT_PROMPT_RESULT::UNFOCUS: parse_event = true; break;
	}
	if(parse_event)
	{
		double temp_double = cvar->data;
		std::string prompt_text = prompt.get_string();
		if(!cvar->cvar_read(prompt_text.c_str()))
		{
			if(!state->error_prompt.init(state, serr_get_error()))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
			if(!state->set_focus(&state->error_prompt))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
		}
		else
		{
			modified = true;

			if(isnan(previous_value))
			{
				previous_value = temp_double;

				// TODO: this is copy pasted
				if(cvar->cvar_type != CVAR_T::RUNTIME)
				{
					switch(cvar->cvar_type)
					{
					case CVAR_T::STARTUP:
						slogf(
							"info: +%s: this value is used in startup, which means that this change requires a restart take effect.\n",
							cvar->cvar_key);
						break;
					case CVAR_T::DEFFERRED:
						slogf(
							"info +%s: this value is deferred, which means that this change will not make immediately take effect.\n",
							cvar->cvar_key);
						break;
					default:
						slogf("info +%s: I don't know why this is here\n", cvar->cvar_key);
						break;
					}
				}
			}

			slider.set_value(cvar->data);
		}
	}
	return modified ? OPTION_ELEMENT_RESULT::MODIFIED : OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_slider_option::draw_buffer()
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	// the description text
	{
		font_painter->begin();
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(0, 0, 0, 255);
		font_painter->set_xy(font_x, font_y);
		font_painter->set_anchor(TEXT_ANCHOR::TOP_LEFT);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->set_style(FONT_STYLE_NORMAL);
		font_painter->set_color(255, 255, 255, 255);
		font_painter->set_xy(font_x, font_y);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->end();
	}

	if(!prompt.draw())
	{
		return false;
	}

	slider.draw_buffer();

	return true;
}

void cvar_slider_option::resize(float x, float y, float menu_w)
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	font_x = x + font_padding / 2.f;
	font_y = y + font_padding / 2.f;

	float cur_x = x + (menu_w - element_padding) / 2;

	float prompt_width = 80 * (font_painter->get_lineskip() / 16.f);
	prompt.set_bbox(
		cur_x - prompt_width, y + font_padding / 2, prompt_width, font_painter->get_lineskip());

	slider.resize_view(cur_x + element_padding, (x + menu_w), y, y + element_height);
}

bool cvar_slider_option::draw_requested()
{
	return prompt.draw_requested() || slider.draw_requested();
}

float cvar_slider_option::get_height()
{
	return element_height;
}

bool cvar_slider_option::set_default()
{
	ASSERT(state != NULL);
	if(isnan(previous_value))
	{
		previous_value = cvar->data;
	}
	if(!cvar->cvar_read(cvar->cvar_default_value.c_str()))
	{
		return false;
	}
	slider.set_value(cvar->data);
	prompt.replace_string(std::to_string(cvar->data).c_str());
	return true;
}
bool cvar_slider_option::undo_changes()
{
	ASSERT(state != NULL);

	if(!isnan(previous_value))
	{
		cvar->data = previous_value;
		slider.set_value(cvar->data);
		prompt.replace_string(std::to_string(cvar->data).c_str());
		previous_value = NAN;
	}

	return true;
}
bool cvar_slider_option::clear_history()
{
	ASSERT(state != NULL);
	previous_value = NAN;
	return true;
}
bool cvar_slider_option::reload_cvars()
{
	ASSERT(state != NULL);
	slider.set_value(cvar->data);
	prompt.replace_string(std::to_string(cvar->data).c_str());
	return true;
}

// a slider + prompt for a floating point number
// clamp will clamp numbers entered into the prompt.
std::unique_ptr<abstract_option_element> create_slider_option(
	shared_cvar_option_state* state, std::string label, cvar_double* cvar, double min, double max)
{
	auto output = std::make_unique<cvar_slider_option>();
	if(!output->init(state, std::move(label), cvar, min, max))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

struct cvar_prompt_option : public abstract_option_element
{
	shared_cvar_option_state* state = NULL;
	std::string label_text;

	V_cvar* cvar = NULL;

	text_prompt_wrapper prompt;

	std::string previous_value;
	bool value_changed = false;

	float element_height = -1;

	float font_x = -1;
	float font_y = -1;

	NDSERR bool init(shared_cvar_option_state* state_, std::string label, V_cvar* cvar_);

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	void resize(float x, float y, float menu_w) override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		// the prompt and slider will unfocus
		// because set_event_hidden will be triggered.
		// but the prompt won't remove the selection so I need to do that here.
		prompt.clear_selection();
		return true;
	};

	float get_height() override;
	NDSERR bool set_default() override;
	NDSERR bool undo_changes() override;
	NDSERR bool clear_history() override;
	NDSERR bool reload_cvars() override;
};

bool cvar_prompt_option::init(shared_cvar_option_state* state_, std::string label, V_cvar* cvar_)
{
	ASSERT(state_ != NULL);
	ASSERT(cvar_ != NULL);

	state = state_;
	label_text = std::move(label);
	cvar = cvar_;

	font_sprite_painter* font_painter = state->font_painter;

	element_height = font_painter->get_lineskip() + state->font_padding;

	// note this only works if you do it before init
	prompt.state.font_scale = font_painter->state.font_scale;

	// TODO(dootsie): I would make the prompt select all the text when you click up without
	// dragging. or just implement double / triple clicking...
	if(!prompt.init(
		   cvar->cvar_write(),
		   font_painter->state.batcher,
		   font_painter->state.font,
		   TEXTP_SINGLE_LINE | TEXTP_DRAW_BBOX | TEXTP_DRAW_BACKDROP | TEXTP_DISABLE_CULL))
	{
		// NOLINTNEXTLINE
		return false;
	}

	return true;
}

bool cvar_prompt_option::update(double delta_sec)
{
	ASSERT(state != NULL);
	// slider.update(delta_sec);
	prompt.update(delta_sec);
	return true;
}
OPTION_ELEMENT_RESULT cvar_prompt_option::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	bool modified = false;

	bool parse_event = false;
	if(prompt.text_focus)
	{
		if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN)
		{
			parse_event = true;
			// eat
			set_event_unfocus(e);
		}
	}
	switch(prompt.input(e))
	{
	// ignore modified because it might be annoying to have an error on every modification.
	// BUT I could make the text change to a red color if the input has an error.
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	case TEXT_PROMPT_RESULT::UNFOCUS: parse_event = true; break;
	}
	if(parse_event)
	{
		std::string temp_string;
		if(!value_changed)
		{
			// if the value is read.
			temp_string = cvar->cvar_write();
		}
		std::string prompt_text = prompt.get_string();
		if(!cvar->cvar_read(prompt_text.c_str()))
		{
			if(!state->error_prompt.init(state, serr_get_error()))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
			if(!state->set_focus(&state->error_prompt))
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
		}
		else
		{
			if(!value_changed)
			{
				previous_value = std::move(temp_string);
				value_changed = true;

				// TODO: this is copy pasted
				if(cvar->cvar_type != CVAR_T::RUNTIME)
				{
					switch(cvar->cvar_type)
					{
					case CVAR_T::STARTUP:
						slogf(
							"info: +%s: this value is used in startup, which means that this change requires a restart take effect.\n",
							cvar->cvar_key);
						break;
					case CVAR_T::DEFFERRED:
						slogf(
							"info +%s: this value is deferred, which means that this change will not make immediately take effect.\n",
							cvar->cvar_key);
						break;
					default:
						slogf("info +%s: I don't know why this is here\n", cvar->cvar_key);
						break;
					}
				}
			}
		}
	}
	return modified ? OPTION_ELEMENT_RESULT::MODIFIED : OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_prompt_option::draw_buffer()
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	// the description text
	{
		font_painter->begin();
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(0, 0, 0, 255);
		font_painter->set_xy(font_x, font_y);
		font_painter->set_anchor(TEXT_ANCHOR::TOP_LEFT);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->set_style(FONT_STYLE_NORMAL);
		font_painter->set_color(255, 255, 255, 255);
		font_painter->set_xy(font_x, font_y);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->end();
	}

	return prompt.draw();
}

void cvar_prompt_option::resize(float x, float y, float menu_w)
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	font_x = x + font_padding / 2.f;
	font_y = y + font_padding / 2.f;

	float cur_x = x + (menu_w - element_padding) / 2;

	float prompt_width = 80 * (font_painter->get_lineskip() / 16.f);
	prompt.set_bbox(
		cur_x + element_padding, y + font_padding / 2, prompt_width, font_painter->get_lineskip());
}

bool cvar_prompt_option::draw_requested()
{
	return prompt.draw_requested();
}

float cvar_prompt_option::get_height()
{
	return element_height;
}

bool cvar_prompt_option::set_default()
{
	ASSERT(state != NULL);
	if(!value_changed)
	{
		previous_value = cvar->cvar_write();
		value_changed = true;
	}
	if(!cvar->cvar_read(cvar->cvar_default_value.c_str()))
	{
		return false;
	}
	prompt.replace_string(cvar->cvar_default_value, false);
	return true;
}
bool cvar_prompt_option::undo_changes()
{
	ASSERT(state != NULL);

	if(value_changed)
	{
		if(!cvar->cvar_read(previous_value.c_str()))
		{
			return false;
		}
		prompt.replace_string(previous_value, false);
		previous_value.clear();
		value_changed = false;
	}

	return true;
}
bool cvar_prompt_option::clear_history()
{
	ASSERT(state != NULL);
	previous_value.clear();
	value_changed = false;
	return true;
}
bool cvar_prompt_option::reload_cvars()
{
	ASSERT(state != NULL);
	prompt.replace_string(cvar->cvar_write());
	return true;
}

std::unique_ptr<abstract_option_element>
	create_prompt_option(shared_cvar_option_state* state, std::string label, V_cvar* cvar)
{
	auto output = std::make_unique<cvar_prompt_option>();
	if(!output->init(state, std::move(label), cvar))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

struct cvar_keybind_option : public abstract_option_element
{
	shared_cvar_option_state* state = NULL;
	cvar_key_bind* cvar = NULL;
	std::string label_text;
	float element_height = -1;

	float font_x = -1;
	float font_y = -1;

	keybind_state previous_key_value;
	bool value_changed = false;
	bool update_buffer = true;

	std::string button_text;
	mono_button_object button;

	NDSERR bool init(shared_cvar_option_state* state_, std::string label, cvar_key_bind* cvar_);

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer() override;
	void resize(float x, float y, float menu_w) override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		return true;
	};

	float get_height() override;
	NDSERR bool set_default() override;
	NDSERR bool undo_changes() override;
	NDSERR bool clear_history() override;
	NDSERR bool reload_cvars() override;
};

bool cvar_keybind_option::init(
	shared_cvar_option_state* state_, std::string label, cvar_key_bind* cvar_)
{
	ASSERT(state_ != NULL);
	ASSERT(cvar_ != NULL);

	state = state_;
	label_text = std::move(label);
	cvar = cvar_;

	button_text = cvar->cvar_write();
	button.init(state->font_painter);

	element_height = state->font_painter->get_lineskip() + state->font_padding;

	return true;
}

bool cvar_keybind_option::update(double delta_sec)
{
	ASSERT(state != NULL);
	button.update(delta_sec);
	return true;
}
OPTION_ELEMENT_RESULT cvar_keybind_option::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	switch(button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		slogf("info: open keybind `%s`\n", label_text.c_str());
		if(!state->keybind_prompt.init(state, this))
		{
			return OPTION_ELEMENT_RESULT::ERROR;
		}
		if(!state->set_focus(&state->keybind_prompt))
		{
			return OPTION_ELEMENT_RESULT::ERROR;
		}
		{
			// dumb hack because the button needs to be told the mouse is obscured.
			SDL_Event e2;
			set_event_leave(e2);
			if(button.input(e2) == BUTTON_RESULT::ERROR)
			{
				return OPTION_ELEMENT_RESULT::ERROR;
			}
		}
		// eat
		set_event_unfocus(e);
		break;
	case BUTTON_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	}
	return OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_keybind_option::draw_buffer()
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	// the description text
	{
		font_painter->begin();
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(0, 0, 0, 255);
		font_painter->set_xy(font_x, font_y);
		font_painter->set_anchor(TEXT_ANCHOR::TOP_LEFT);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->set_style(FONT_STYLE_NORMAL);
		font_painter->set_color(255, 255, 255, 255);
		font_painter->set_xy(font_x, font_y);
		if(!font_painter->draw_text(label_text.c_str(), label_text.size()))
		{
			return false;
		}
		font_painter->end();
	}

	if(!button.draw_buffer(button_text.c_str(), button_text.size()))
	{
		return false;
	}

	update_buffer = false;

	return true;
}

void cvar_keybind_option::resize(float x, float y, float menu_w)
{
	ASSERT(state != NULL);

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	font_x = x + font_padding / 2.f;
	font_y = y + font_padding / 2.f;

	float cur_x = x + (menu_w - element_padding) / 2 + element_padding;
	button.set_rect(cur_x, y, (x + menu_w) - cur_x, element_height);
}

bool cvar_keybind_option::draw_requested()
{
	return update_buffer || button.draw_requested();
}

float cvar_keybind_option::get_height()
{
	return element_height;
}

bool cvar_keybind_option::set_default()
{
	if(!value_changed)
	{
		previous_key_value = cvar->key_binds;
		value_changed = true;
	}
	if(!cvar->cvar_read(cvar->cvar_default_value.c_str()))
	{
		return false;
	}

	button_text = cvar->cvar_write();
	return true;
}
bool cvar_keybind_option::undo_changes()
{
	ASSERT(state != NULL);

	if(value_changed)
	{
		cvar->key_binds = previous_key_value;
		value_changed = false;
		button_text = cvar->cvar_write();
	}
	return true;
}
bool cvar_keybind_option::clear_history()
{
	ASSERT(state != NULL);
	value_changed = false;
	return true;
}
bool cvar_keybind_option::reload_cvars()
{
	ASSERT(state != NULL);
	button_text = cvar->cvar_write();
	return true;
}

std::unique_ptr<abstract_option_element>
	create_keybind_option(shared_cvar_option_state* state, std::string label, cvar_key_bind* cvar)
{
	auto output = std::make_unique<cvar_keybind_option>();
	if(!output->init(state, std::move(label), cvar))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

bool option_keybind_request::init(
	shared_cvar_option_state* state_, cvar_keybind_option* option_state_)
{
	ASSERT(state_ != NULL);
	ASSERT(option_state_ != NULL);

	state = state_;
	option_state = option_state_;

	temp_value = option_state->cvar->key_binds;
	value_modified = false;
	update_buffer = true;

	gl_batch_buffer_offset = 0;
	batch_vertex_count = 0;
	gl_batch_buffer_offset = -1;
	batch_vertex_count = 0;

	// TODO: should be font_painter.init(state->font_painter)
	font_painter.state = state->font_painter->state;
	font_painter.set_flags(TEXT_FLAGS::NEWLINE);

	unbind_button_text = "unbind";
	unbind_button.init(&font_painter);

	cancel_button_text = "cancel";
	cancel_button.init(&font_painter);

	ok_button_text = "ok";
	ok_button.init(&font_painter);
	ok_button.set_disabled(true);

	return format_text();
}

bool option_keybind_request::format_text()
{
	display_message = unique_asprintf(
		&display_message_len,
		"bind: %s\n"
		"key: %s\n"
		"press any button to modify",
		option_state->label_text.c_str(),
		option_state->cvar->keybind_to_string(temp_value).c_str());

	update_buffer = true;

	return display_message &&
		   font_painter.measure_text_bounds(
			   display_message.get(), display_message_len, &text_width, &text_height);
}

void option_keybind_request::resize_view()
{
	ASSERT(state != NULL);

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	// for a 16px font I would want 60px
	float button_width = 60 * (font_painter.get_lineskip() / 16.f);
	float button_height = font_painter.get_lineskip() + font_padding;
	float footer_width = (button_width * 2) + element_padding;
	float footer_height = button_height;

	// for a 16px font I would want 400px
	float menu_width = std::max(text_width, footer_width);
	float menu_height = text_height + element_padding + footer_height;

	float screen_width = static_cast<float>(cv_screen_width.data);
	float screen_height = static_cast<float>(cv_screen_height.data);

	float x = std::floor((screen_width - menu_width) / 2.f);
	float y = std::floor((screen_height - menu_height) / 2.f);

	// footer buttons
	{
		float x_cursor = x + menu_width;
		x_cursor -= button_width; //+ element_padding;
		ok_button.set_rect(
			x_cursor, y + text_height + element_padding, button_width, button_height);
		x_cursor -= button_width + element_padding;
		cancel_button.set_rect(
			x_cursor, y + text_height + element_padding, button_width, button_height);
		x_cursor -= button_width + element_padding;
		unbind_button.set_rect(
			x_cursor, y + text_height + element_padding, button_width, button_height);
	}

	box_xmin = x - element_padding;
	box_xmax = x + menu_width + element_padding;
	box_ymin = y - element_padding;
	box_ymax = y + menu_height + element_padding;
}

bool option_keybind_request::update(double delta_sec)
{
	ASSERT(state != NULL);
	ok_button.update(delta_sec);
	cancel_button.update(delta_sec);
	unbind_button.update(delta_sec);
	return true;
}
void option_keybind_request::commit_change()
{
	if(!option_state->value_changed)
	{
		option_state->previous_key_value = option_state->cvar->key_binds;
		option_state->value_changed = true;
	}
	option_state->cvar->key_binds = temp_value;
	option_state->button_text = option_state->cvar->cvar_write();
	option_state->update_buffer = true;
}
FOCUS_ELEMENT_RESULT option_keybind_request::input(SDL_Event& e)
{
	ASSERT(state != NULL);

	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
		case SDL_WINDOWEVENT_HIDDEN:
		case SDL_WINDOWEVENT_FOCUS_LOST:
			// some event was eaten (like opening up the console),
			// or click'd out, so lets just cancel the keybind.
			return FOCUS_ELEMENT_RESULT::CLOSE;
		}
	}

	switch(unbind_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		value_modified = true;
		temp_value.type = KEYBIND_T::NONE;
		ok_button.set_disabled(false);
		if(!format_text())
		{
			return FOCUS_ELEMENT_RESULT::ERROR;
		}
		// eat
		set_event_unfocus(e);
		break;
	case BUTTON_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
	}

	switch(cancel_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER: return FOCUS_ELEMENT_RESULT::CLOSE;
	case BUTTON_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
	}

	switch(ok_button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		if(value_modified)
		{
			commit_change();
			return FOCUS_ELEMENT_RESULT::MODIFIED;
		}
		return FOCUS_ELEMENT_RESULT::CLOSE;
	case BUTTON_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
	}

	/*if(e.type == SDL_KEYDOWN)
	{
		switch(e.key.keysym.sym)
		{
		case SDLK_ESCAPE:
		case SDLK_RETURN:
			if(value_modified)
			{
				commit_change();
				return FOCUS_ELEMENT_RESULT::MODIFIED;
			}
			return FOCUS_ELEMENT_RESULT::CLOSE;
		}
	}
	*/

	keybind_state out;
	if(option_state->cvar->bind_sdl_event(out, e))
	{
		value_modified = true;
		ok_button.set_disabled(false);
		temp_value = out;
		if(!format_text())
		{
			return FOCUS_ELEMENT_RESULT::ERROR;
		}
		set_event_unfocus(e);
	}

	// backdrop
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);
		// helps unfocus other elements.
		if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x && box_xmin <= mouse_x)
		{
			// eat
			set_event_leave(e);
			return FOCUS_ELEMENT_RESULT::CONTINUE;
		}
	}
	break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);
			// helps unfocus other elements.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				// eat
				set_event_unfocus(e);
				return FOCUS_ELEMENT_RESULT::CONTINUE;
			}
		}
		break;
	}

	return FOCUS_ELEMENT_RESULT::CONTINUE;
}
bool option_keybind_request::draw_buffer()
{
	ASSERT(state != NULL);

	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

	gl_batch_buffer_offset = batcher->get_current_vertex_count();

	resize_view();

	// backdrop
	{
		float xmin = box_xmin;
		float xmax = box_xmax;
		float ymin = box_ymin;
		float ymax = box_ymax;

		std::array<uint8_t, 4> fill_color = RGBA8_PREMULT(50, 50, 50, 200);

		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}

	font_painter.begin();

	float font_x = box_xmin + state->element_padding;
	float font_y = box_ymin + state->element_padding;

	font_painter.begin();
	font_painter.set_style(FONT_STYLE_OUTLINE);
	font_painter.set_color(0, 0, 0, 255);
	font_painter.set_xy(font_x, font_y);
	font_painter.set_anchor(TEXT_ANCHOR::TOP_LEFT);
	if(!font_painter.draw_text(display_message.get(), display_message_len))
	{
		return false;
	}
	font_painter.set_style(FONT_STYLE_NORMAL);
	font_painter.set_color(255, 255, 255, 255);
	font_painter.set_xy(font_x, font_y);
	if(!font_painter.draw_text(display_message.get(), display_message_len))
	{
		return false;
	}
	font_painter.end();

	if(!ok_button.draw_buffer(ok_button_text.c_str(), ok_button_text.size()))
	{
		return false;
	}

	if(!cancel_button.draw_buffer(cancel_button_text.c_str(), cancel_button_text.size()))
	{
		return false;
	}

	if(!unbind_button.draw_buffer(unbind_button_text.c_str(), unbind_button_text.size()))
	{
		return false;
	}

	batch_vertex_count = batcher->get_current_vertex_count();

	update_buffer = false;

	return true;
}
bool option_keybind_request::render()
{
	ASSERT(state != NULL);
	ASSERT(gl_batch_buffer_offset != -1);

	if(batch_vertex_count - gl_batch_buffer_offset > 0)
	{
		ctx.glDrawArrays(
			GL_TRIANGLES, gl_batch_buffer_offset, batch_vertex_count - gl_batch_buffer_offset);
	}

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
bool option_keybind_request::draw_requested()
{
	ASSERT(state != NULL);
	return update_buffer || ok_button.draw_requested() || cancel_button.draw_requested() ||
		   unbind_button.draw_requested();
}

bool option_keybind_request::close()
{
	return true;
}