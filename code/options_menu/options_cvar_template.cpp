#include "../global.h"
#include "options_cvar_template.h"

#include "../ui.h"
#include "../font/text_prompt.h"
#include "../font/utf8_stuff.h"
#include "../app.h"

bool option_error_prompt::init(shared_cvar_option_state* state_, std::string message)
{
    ASSERT(state_ != NULL);

    state = state_;
	display_message = std::move(message);

	font_painter.init(state->font_painter->state.batcher, state->font_painter->state.font);
    font_painter.set_flags(TEXT_FLAGS::NEWLINE);

    ok_button_text = "ok";
	ok_button.init(&font_painter);

	return font_painter.measure_text_bounds(
		display_message.c_str(), display_message.size(), &text_width, &text_height);
}

void option_error_prompt::resize_view()
{
    ASSERT(state != NULL);

    float font_padding = state->font_padding;
    float element_padding = state->element_padding;

    // for a 16px font I would want 60px
    float button_width = 60 * (font_painter.get_lineskip() / 16.f);
    float button_height = font_painter.get_lineskip() + font_padding;
    float footer_width = button_width;
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
			x_cursor,
			y + text_height + element_padding,
			button_width,
			button_height);
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
    return true;
}
FOCUS_ELEMENT_RESULT option_error_prompt::input(SDL_Event& e)
{
    ASSERT(state != NULL);
    switch(ok_button.input(e))
    {
    case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		return FOCUS_ELEMENT_RESULT::CLOSE;
	case BUTTON_RESULT::ERROR: return FOCUS_ELEMENT_RESULT::ERROR;
    }

	if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
	{
		return FOCUS_ELEMENT_RESULT::CLOSE;
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

	mono_2d_batcher* batcher = font_painter.state.batcher;
	auto white_uv = font_painter.state.font->get_font_atlas()->white_uv;
	std::array<uint8_t, 4> bbox_color{0, 0, 0, 255};

    gl_batch_buffer_offset = batcher->get_current_vertex_count();

    resize_view();

    //backdrop
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
    if(!font_painter.draw_text(display_message.c_str(), display_message.size()))
    {
        return false;
    }
    font_painter.set_style(FONT_STYLE_NORMAL);
    font_painter.set_color(255, 255, 255, 255);
    font_painter.set_xy(font_x, font_y);
    if(!font_painter.draw_text(display_message.c_str(), display_message.size()))
    {
        return false;
    }
    font_painter.end();

    if(!ok_button.draw_buffer(ok_button_text.c_str(), ok_button_text.size()))
    {
        // NOLINTNEXTLINE
        return false;
    }

	batch_vertex_count = batcher->get_current_vertex_count();

    return true;
}
bool option_error_prompt::render()
{
    ASSERT(state != NULL);
    ASSERT(gl_batch_buffer_offset != -1);

    if(batch_vertex_count - gl_batch_buffer_offset > 0)
	{
        // optimization: some render() calls use a draw call, and some dont,
        // This is a microoptimization because focus_element isn't a hot path (only called once)
        // but maybe I might want options to be more complex and elements could have multiple draw calls.
        // maybe the render() call should turn into render(GLint *offset, GLsizei *count),
        // if you don't need a exclusive draw call, just append *count.
        // if you do, then you must draw the previous batch (because it's likely you need
        // a exclusive draw call because you are using glScissor), then use your exclusive draw.
        // and then after drawing do *offset += *count; *count = 0;
        // then when all the elements are draw, make sure to complete the last draw call.
		ctx.glDrawArrays(GL_TRIANGLES, gl_batch_buffer_offset, batch_vertex_count - gl_batch_buffer_offset);
	}

	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
bool option_error_prompt::draw_requested()
{
    ASSERT(state != NULL);

    return ok_button.draw_requested();
}


// this is mainly for on or off buttons, but you can set this to
struct cvar_button_option : public abstract_option_element
{
	shared_cvar_option_state* state = NULL;
	cvar_int* cvar = NULL;
	std::string label_text;
	float element_height = -1;
	int previous_int_value = -1;
	bool value_changed = false;

	std::unique_ptr<multi_option_entry[]> option_entries;
	size_t option_entries_size = 0;

	size_t current_button_index = 0;

	mono_button_object button;

	NDSERR bool init(
		shared_cvar_option_state* state_,
		std::string label,
		cvar_int* cvar_,
		size_t count,
		multi_option_entry* entries);

	void set_error_button();

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer(float x, float y, float menu_w) override;
	bool draw_requested() override;
	NDSERR bool close() override
	{
		return true;
	};

	float get_height() override;
	NDSERR bool set_default() override;
	NDSERR bool undo_changes() override;
	NDSERR bool clear_history() override;
};

bool cvar_button_option::init(
	shared_cvar_option_state* state_,
	std::string label,
	cvar_int* cvar_,
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
	option_entries_size = count;
	bool found = false;
	for(size_t i = 0; i < option_entries_size; ++i)
	{
		option_entries[i] = std::move(entries[i]);
		if(option_entries[i].value == cvar->data)
		{
			ASSERT(!found);
			found = true;
			current_button_index = i;
		}
	}
	ASSERT(found);
	if(!found)
	{
		slogf(
			"info: couldn't find a option with the current value of the cvar %s (%d).\n",
			cvar->cvar_key,
			cvar->data);
		set_error_button();
	}

	return true;
}

void cvar_button_option::set_error_button()
{
	button.disabled = true;
	current_button_index = 0;
	option_entries_size = 1;
	option_entries[0].value = 0;
	option_entries[0].name = "?";
}

bool cvar_button_option::update(double delta_sec)
{
	ASSERT(state != NULL);
	button.update(delta_sec);
	return true;
}
OPTION_ELEMENT_RESULT cvar_button_option::input(SDL_Event& e)
{
	ASSERT(state != NULL);
	switch(button.input(e))
	{
	case BUTTON_RESULT::CONTINUE: break;
	case BUTTON_RESULT::TRIGGER:
		if(!value_changed)
		{
			previous_int_value = cvar->data;
			value_changed = true;
		}
		current_button_index = (current_button_index + 1) % option_entries_size;
		cvar->data = option_entries[current_button_index].value;
		return OPTION_ELEMENT_RESULT::MODIFY;
	case BUTTON_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	}
	return OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_button_option::draw_buffer(float x, float y, float menu_w)
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	// the description text
	{
		float font_x = x + font_padding / 2.f;
		float font_y = y + font_padding / 2.f;

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

	ASSERT(current_button_index < option_entries_size);
	const char* text = option_entries[current_button_index].name.c_str();
	size_t text_size = option_entries[current_button_index].name.size();
	float cur_x = x + (menu_w - element_padding) / 2 + element_padding;
	button.set_rect(cur_x, y, (x + menu_w) - cur_x, element_height);
	return button.draw_buffer(text, text_size);
}

bool cvar_button_option::draw_requested()
{
	return button.draw_requested();
}

float cvar_button_option::get_height()
{
	return element_height;
}

bool cvar_button_option::set_default()
{
	ASSERT(state != NULL);
	if(!value_changed)
	{
		previous_int_value = cvar->data;
		value_changed = true;
	}
	if(!cvar->cvar_read(cvar->cvar_default_value.c_str()))
	{
		return false;
	}
	bool found = false;
	for(size_t i = 0; i < option_entries_size; ++i)
	{
		if(option_entries[i].value == cvar->data)
		{
			ASSERT(!found);
			found = true;
			current_button_index = i;
		}
	}
	ASSERT(found);
	if(!found)
	{
		slogf(
			"info: couldn't find a option with the current value of the cvar %s (%d).\n",
			cvar->cvar_key,
			cvar->data);
		set_error_button();
	}
	return true;
}
bool cvar_button_option::undo_changes()
{
	ASSERT(state != NULL);

	if(value_changed)
	{
		// TODO: for special inherited cvar_int cvars like vsync & fullscreen
		// I could try to implement a on_modify() virtual function (or *operator= )
		cvar->data = previous_int_value;
		previous_int_value = -1;
		value_changed = false;
	}
	bool found = false;
	for(size_t i = 0; i < option_entries_size; ++i)
	{
		if(option_entries[i].value == cvar->data)
		{
			ASSERT(!found);
			found = true;
			current_button_index = i;
		}
	}
	ASSERT(found);
	if(!found)
	{
		slogf(
			"info: couldn't find a option with the current value of the cvar %s (%d).\n",
			cvar->cvar_key,
			cvar->data);
		set_error_button();
	}

	return true;
}
bool cvar_button_option::clear_history()
{
	ASSERT(state != NULL);
	previous_int_value = -1;
	value_changed = false;
	return true;
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

	bool clamp_prompt = false;

	NDSERR bool init(
		shared_cvar_option_state* state_,
		std::string label,
		cvar_double* cvar_,
		double min,
		double max,
		bool clamp);

	// virtual functions
	NDSERR bool update(double delta_sec) override;
	NDSERR OPTION_ELEMENT_RESULT input(SDL_Event& e) override;
	NDSERR bool draw_buffer(float x, float y, float menu_w) override;
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
};

bool cvar_slider_option::init(
	shared_cvar_option_state* state_,
	std::string label,
	cvar_double* cvar_,
	double min,
	double max,
	bool clamp)
{
	ASSERT(state_ != NULL);
	ASSERT(cvar_ != NULL);

	state = state_;
	label_text = std::move(label);
	cvar = cvar_;

	min_value = min;
	max_value = max;
	clamp_prompt = clamp;

	font_sprite_painter* font_painter = state->font_painter;

	element_height = font_painter->get_lineskip() + state->font_padding;

	slider.init(font_painter, cvar->data);

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
		modified = true;
		if(isnan(previous_value))
		{
			previous_value = cvar->data;
		}
		cvar->data = slider.get_value();
		prompt.replace_string(std::to_string(cvar->data).c_str());
	}

	bool parse_event = false;
    if(prompt.text_focus){
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
	case TEXT_PROMPT_RESULT::MODIFIED:
	case TEXT_PROMPT_RESULT::CONTINUE: break;
	case TEXT_PROMPT_RESULT::ERROR: return OPTION_ELEMENT_RESULT::ERROR;
	case TEXT_PROMPT_RESULT::UNFOCUS: parse_event = true; break;
	}
	if(parse_event)
	{
        std::istringstream iss(prompt.get_string());
        double out;
        if(!(iss >> out))
        {
            slogf("convert error\n");
            // TODO: give more info
            //serrf("failed to convert number.\n");
            //prompt.replace_string(std::to_string(cvar->data));
            if(!state->error_prompt.init(state, "failed to convert number."))
            {
                return OPTION_ELEMENT_RESULT::ERROR;
            }
            if(!state->set_focus(&state->error_prompt))
            {
                return OPTION_ELEMENT_RESULT::ERROR;
            }
            // eat
            set_event_unfocus(e);
            return OPTION_ELEMENT_RESULT::CONTINUE;
        }

        if(clamp_prompt && (out < min_value || out > max_value))
        {
            slogf("range error\n");
            // TODO: give more info
            if(!state->error_prompt.init(state, "failed to convert number range."))
            {
                return OPTION_ELEMENT_RESULT::ERROR;
            }
            
            if(!state->set_focus(&state->error_prompt))
            {
                return OPTION_ELEMENT_RESULT::ERROR;
            }
            // eat
            set_event_unfocus(e);
            return OPTION_ELEMENT_RESULT::CONTINUE;
        }
        modified = true;
        if(isnan(previous_value))
        {
            previous_value = cvar->data;
        }
        cvar->data = out;
        slider.set_value(out);

        // eat
        set_event_unfocus(e);
        return OPTION_ELEMENT_RESULT::CONTINUE;
		
	}
	return modified ? OPTION_ELEMENT_RESULT::MODIFY : OPTION_ELEMENT_RESULT::CONTINUE;
}
bool cvar_slider_option::draw_buffer(float x, float y, float menu_w)
{
	ASSERT(state != NULL);
	font_sprite_painter* font_painter = state->font_painter;

	float font_padding = state->font_padding;
	float element_padding = state->element_padding;

	// the description text
	{
		float font_x = x + font_padding / 2.f;
		float font_y = y + font_padding / 2.f;

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
	// float cur_x = x + (menu_w - element_padding) / 2 + element_padding;
	// button.set_rect(cur_x, y, (x + menu_w) - cur_x, element_height);
	// return button.draw_buffer(button_text.c_str(), button_text.size());

	float cur_x = x + (menu_w - element_padding) / 2;

	float prompt_width = 80 * (font_painter->get_lineskip() / 16.f);
	prompt.set_bbox(
		cur_x - prompt_width,
		y + font_padding / 2,
		prompt_width,
		font_painter->get_lineskip());
	// this is neccessary, but we need to draw every frame.
	prompt.draw_requested();
	if(!prompt.draw())
	{
		return false;
	}
	slider.resize_view(cur_x + element_padding, (x + menu_w), y, y + element_height);
	slider.draw_buffer();

	return true;
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

std::unique_ptr<abstract_option_element>
	create_bool_option(shared_cvar_option_state* state, std::string label, cvar_int* cvar)
{
	multi_option_entry bool_options[] = {{0, "off"}, {1, "on"}};
	auto output = std::make_unique<cvar_button_option>();
	if(!output->init(state, std::move(label), cvar, std::size(bool_options), bool_options))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

std::unique_ptr<abstract_option_element> create_multi_option(
	shared_cvar_option_state* state,
	std::string label,
	cvar_int* cvar,
	size_t count,
	multi_option_entry* entries)
{
	auto output = std::make_unique<cvar_button_option>();
	if(!output->init(state, std::move(label), cvar, count, entries))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}

std::unique_ptr<abstract_option_element> create_int_prompt_option(
	shared_cvar_option_state* state,
	std::string label,
	cvar_double* cvar,
	int min,
	int max,
	bool clamp)
{
}

// a slider + prompt for a floating point number
// clamp will clamp numbers entered into the prompt.
std::unique_ptr<abstract_option_element> create_slider_option(
	shared_cvar_option_state* state,
	std::string label,
	cvar_double* cvar,
	double min,
	double max,
	bool clamp)
{
	auto output = std::make_unique<cvar_slider_option>();
	if(!output->init(state, std::move(label), cvar, min, max, clamp))
	{
		return std::unique_ptr<abstract_option_element>();
	}
	return output;
}