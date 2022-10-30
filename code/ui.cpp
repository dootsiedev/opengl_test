#include "global_pch.h"
#include "global.h"

#include "ui.h"

#include "app.h"

// TODO: maybe make the button get highlighted when you press down on it?

/*
void set_event_leave(SDL_Event& e)
{
	if(e.type == SDL_MOUSEMOTION)
	{
		// this is a hack to allow dragging a slider to still work on top of "leave" events.
		e.motion.windowID = CLIPPED_WINDOW_ID;
		return;
	}
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_LEAVE;
	e.window.windowID = 0;
}
*/

void set_event_unfocus(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
	e.window.windowID = 0;
}

void set_event_resize(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
	e.window.windowID = 0;
}

void set_event_hidden(SDL_Event& e)
{
	e.type = SDL_WINDOWEVENT;
	e.window.event = SDL_WINDOWEVENT_HIDDEN;
	e.window.windowID = 0;
}

// UNRELATED: I wonder if it would be very stupid if I used this for keyboard events,
// because I have a problem where I would wasd move the camera while navigating a menu,
// but every time I click up or down, the unfocus event would hitch my camera movement.
// maybe instead I should add in DOOT_EATEN_SDL_KEYUP & DOOT_CLIPPED_SDL_MOUSEMOTION.
// but the only problem with that removing is_mouse_event_clipped() requires refactoring...
// and ideally I would want a .clipped member in .motion & .button
void set_mouse_event_clipped(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_MOUSEMOTION: e.motion.windowID = CLIPPED_WINDOW_ID; return;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP: e.button.windowID = CLIPPED_WINDOW_ID; return;
	}
	ASSERT(false && "not a mouse event");
}

bool is_mouse_event_clipped(SDL_Event& e)
{
	switch(e.type)
	{
	case SDL_MOUSEMOTION: return e.motion.windowID == CLIPPED_WINDOW_ID;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP: return e.button.windowID == CLIPPED_WINDOW_ID;
	}
	return false;
}

bool is_unfocus_event_text_input_stolen(SDL_Event& e)
{
	if(e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
	{
		return e.window.windowID == TEXT_INPUT_STOLEN_WINDOW_ID;
	}
	return false;
}

BUTTON_RESULT mono_button_object::input(SDL_Event& e)
{
	ASSERT(font_painter != NULL);

	if(e.type == SDL_WINDOWEVENT)
	{
		switch(e.window.event)
		{
			// release "hover focus"
		case SDL_WINDOWEVENT_LEAVE:
			hover_over = false;
			return BUTTON_RESULT::CONTINUE;
			// if a the UI disappears.
		case SDL_WINDOWEVENT_HIDDEN:
			clicked_on = false;
			hover_over = false;
			// note this is neccessary because
			// I treat set_event_hidden as "close this menu in a state it can be reopened"
			// which means any fade effects would appear when you reopen the menu
			// because all update()'s would stop after the menu is closed.
			fade = 0;
			pop_effect = 0;
			return BUTTON_RESULT::CONTINUE;

			// if you unhovered from SDL_WINDOWEVENT_FOCUS_LOST,
			// pressing keys in a prompt would make the mouse hover go away!
			// case SDL_WINDOWEVENT_FOCUS_LOST:
		}
	}

	if(disabled)
	{
		return BUTTON_RESULT::CONTINUE;
	}

	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		if(clicked_on && (e.motion.state & SDL_BUTTON_LMASK) == 0 && (e.motion.state & SDL_BUTTON_RMASK) == 0)
		{
			clicked_on = false;
		}
		if(is_mouse_event_clipped(e))
		{
			// clipped shouldn't cause hover
			hover_over = false;
			break;
		}
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];
		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			hover_over = true;
			// eat
			set_mouse_event_clipped(e);
			return BUTTON_RESULT::CONTINUE;
		}
		hover_over = false;
	}
	break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			if(is_mouse_event_clipped(e))
			{
				clicked_on = false;
				break;
			}
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			float xmin = button_rect[0];
			float xmax = button_rect[0] + button_rect[2];
			float ymin = button_rect[1];
			float ymax = button_rect[1] + button_rect[3];

			if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
			{
				clicked_on = true;
				// eat
				set_event_unfocus(e);
				return BUTTON_RESULT::CONTINUE;
			}
		}
		clicked_on = false;
		break;
	case SDL_MOUSEBUTTONUP:
		if(clicked_on)
		{
			clicked_on = false;
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);

				float xmin = button_rect[0];
				float xmax = button_rect[0] + button_rect[2];
				float ymin = button_rect[1];
				float ymax = button_rect[1] + button_rect[3];

				// eat
				set_event_unfocus(e);

				if(!is_mouse_event_clipped(e) && ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
				{
					// slog("click\n");
					// reset the fade  to .5 for an effect
					pop_effect = 1.f;
					update_buffer = true;
					return BUTTON_RESULT::TRIGGER;
				}
			}
		}
		break;
	}

	return BUTTON_RESULT::CONTINUE;
}
void mono_button_object::update(double delta_sec)
{
	ASSERT(font_painter != NULL);

	if(pop_effect > 0)
	{
		pop_effect -= static_cast<float>(delta_sec) * 4;
		update_buffer = true;
		fade = 1.f;
	}
	else
	{
		auto prev_fade = fade;

		// add fade
		fade += static_cast<float>(hover_over ? delta_sec : -delta_sec) * color_state.fade_speed;

		// clamp
		fade = std::min(fade, 1.f);
		fade = std::max(fade, 0.f);

		if(fade != prev_fade)
		{
			update_buffer = true;
		}
	}
}
bool mono_button_object::draw_buffer(const char* button_text, size_t button_text_len)
{
	ASSERT(font_painter != NULL);

	update_buffer = false;

	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	std::array<uint8_t, 4> color1;
	std::array<uint8_t, 4> color2;
	float mix;

	if(pop_effect > 0)
	{
		color1 = color_state.click_pop_fill_color;
		color2 = color_state.hot_fill_color;
		mix = pop_effect;
	}
	else
	{
		color1 = color_state.hot_fill_color;
		color2 = color_state.idle_fill_color;
		mix = fade;
	}

	// normalize the colors 0-1
	float hot_fill[4] = {
		static_cast<float>(color1[0]) / 255.f,
		static_cast<float>(color1[1]) / 255.f,
		static_cast<float>(color1[2]) / 255.f,
		static_cast<float>(color1[3]) / 255.f};
	float idle_fill[4] = {
		static_cast<float>(color2[0]) / 255.f,
		static_cast<float>(color2[1]) / 255.f,
		static_cast<float>(color2[2]) / 255.f,
		static_cast<float>(color2[3]) / 255.f};
	// blend the colors.
	std::array<uint8_t, 4> fill_color = {
		static_cast<uint8_t>((hot_fill[0] * mix + idle_fill[0] * (1.f - mix)) * 255.f),
		static_cast<uint8_t>((hot_fill[1] * mix + idle_fill[1] * (1.f - mix)) * 255.f),
		static_cast<uint8_t>((hot_fill[2] * mix + idle_fill[2] * (1.f - mix)) * 255.f),
		static_cast<uint8_t>((hot_fill[3] * mix + idle_fill[3] * (1.f - mix)) * 255.f)};

	// backdrop
	{
		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];

		// fill
		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, fill_color);

		// bbox
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, color_state.bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, color_state.bbox_color);
	}

	// font
	font_painter->begin();
	font_painter->set_anchor(TEXT_ANCHOR::CENTER_PERFECT);

	float font_x = button_rect[0] + (button_rect[2] / 2.f);
	float font_y = button_rect[1] + (button_rect[3] / 2.f);

	if(color_state.text_outline)
	{
		// outline
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(color_state.text_outline_color);
		font_painter->set_xy(font_x, font_y);
		if(!font_painter->draw_text(button_text, button_text_len))
		{
			return false;
		}
	}

	font_painter->set_style(FONT_STYLE_NORMAL);
	font_painter->set_color(disabled ? color_state.disabled_text_color : color_state.text_color);
	font_painter->set_xy(font_x, font_y);
	if(!font_painter->draw_text(button_text, button_text_len))
	{
		return false;
	}

	font_painter->end();

	// there are not much gl calls here, but the text does modify the atlas.
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
//
// scrollable
//
void mono_y_scrollable_area::unfocus()
{
	y_scrollbar_held = false;
	scroll_thumb_click_offset = -1;
}
void mono_y_scrollable_area::resize_view(float xmin, float xmax, float ymin, float ymax)
{
	box_xmin = xmin;
	box_xmax = xmax;
	box_ymin = ymin;
	box_ymax = ymax;
	// probably should use content_h > (ymax-ymin), but this feels more stable
	box_inner_xmax = xmax - scrollbar_thickness - scrollbar_padding;
	// clamp the scroll (when the screen resizes)
	scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));

	update_buffer = true;
}

SCROLLABLE_AREA_RETURN mono_y_scrollable_area::input(SDL_Event& e)
{
	ASSERT(font_painter != NULL);

	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
			// release scrollbar focus.
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			unfocus();
			break;
			// leave is only used for releasing "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
	}

	bool modified = false;

	if(content_h > (box_ymax - box_ymin))
	{
		switch(e.type)
		{
		// lazy scroll
		case SDL_MOUSEWHEEL: {
			// only scroll when the mouse is currently hovering over the bounding box
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			float mouse_x = static_cast<float>(x);
			float mouse_y = static_cast<float>(y);

			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				scroll_y -= static_cast<float>(e.wheel.y * cv_scroll_speed.data) *
							font_painter->get_lineskip();
				// clamp
				scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
			}
			modified = true;
			// I use this to prevent other things from scrolling,
			// and because there might be something I might be hovering over inside
			// the scrollable area, and it needs to be un-hover-focused.
			// because this isn't a motion event, I need to fix the values.
			e.type = SDL_MOUSEMOTION;
			e.motion.x = x;
			e.motion.y = y;
			set_mouse_event_clipped(e);
		}
		break;
		case SDL_MOUSEMOTION: {
			float mouse_x = static_cast<float>(e.motion.x);
			float mouse_y = static_cast<float>(e.motion.y);
			if(y_scrollbar_held)
			{
				if((e.motion.state & SDL_BUTTON_LMASK) == 0 && (e.motion.state & SDL_BUTTON_RMASK) == 0)
				{
					y_scrollbar_held = false;
				}
				else
				{
					internal_scroll_y_to(mouse_y);
					modified = true;
					break;
				}
			}
			// helps unfocus other elements.
			if(internal_scroll_y_inside(mouse_x, mouse_y))
			{
				// eat hover
				set_mouse_event_clipped(e);
				break;
			}
		}
		break;
		case SDL_MOUSEBUTTONUP:
			// TODO: I know that there are other places I treat SDL_BUTTON_RIGHT as LMB
			// but I am thinking of making RMB used only for escaping out of hovered menus
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				// float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);
				if(y_scrollbar_held)
				{
					internal_scroll_y_to(mouse_y);
					modified = true;
					y_scrollbar_held = false;
					scroll_thumb_click_offset = -1;
					// eat
					set_event_unfocus(e);
				}
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				if(is_mouse_event_clipped(e))
				{
					unfocus();
					break;
				}
				float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);

				if(internal_scroll_y_inside(mouse_x, mouse_y))
				{
					y_scrollbar_held = true;
					// eat
					set_event_unfocus(e);
					break;
				}

				unfocus();
			}
			break;
		}
	}
	return modified ? SCROLLABLE_AREA_RETURN::MODIFIED : SCROLLABLE_AREA_RETURN::CONTINUE;
}

void mono_y_scrollable_area::draw_buffer()
{
	ASSERT(font_painter != NULL);
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	if(content_h > (box_ymax - box_ymin))
	{
		// draw the scrollbar bbox
		{
			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin;
			float ymax = box_ymax;

			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
		// draw the scrollbar thumb
		{
			float scrollbar_max_height = (box_ymax - box_ymin);

			float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
			thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

			float scroll_ratio =
				(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
			float thumb_offset = scroll_y * scroll_ratio;

			float xmin = box_xmax - scrollbar_thickness;
			float xmax = box_xmax;
			float ymin = box_ymin + thumb_offset;
			float ymax = box_ymin + thumb_offset + thumb_height;

			batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
			batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
			batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
			batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
		}
	}
	update_buffer = false;
}

bool mono_y_scrollable_area::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio =
		(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
	float thumb_offset = scroll_y * scroll_ratio;

	float xmin = box_xmax - scrollbar_thickness;
	float xmax = box_xmax;
	float ymin = box_ymin + thumb_offset;
	float ymax = box_ymin + thumb_offset + thumb_height;

	if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
	{
		scroll_thumb_click_offset = mouse_y - thumb_offset;
		return true;
	}

	return false;
}

void mono_y_scrollable_area::internal_scroll_y_to(float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio =
		(scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
	scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;

	// clamp
	scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
}

void mono_normalized_slider_object::init(
	font_sprite_painter* font_painter_,
	double initial_value,
	double slider_min_,
	double slider_max_)
{
	ASSERT(font_painter_ != NULL);
	slider_value = initial_value;
	font_painter = font_painter_;
	slider_min = slider_min_;
	slider_max = slider_max_;
}

bool mono_normalized_slider_object::input(SDL_Event& e)
{
	ASSERT(font_painter != NULL);

	switch(e.type)
	{
	case SDL_WINDOWEVENT:
		switch(e.window.event)
		{
			// release scrollbar focus.
		case SDL_WINDOWEVENT_FOCUS_LOST:
		case SDL_WINDOWEVENT_HIDDEN:
			unfocus();
			return false;
			// leave is only used for releasing "hover focus"
			// case SDL_WINDOWEVENT_LEAVE:
		}
	}

	switch(e.type)
	{
	// I don't want a scroll because if the silder is inside a scrollable area (its not)
	// I accidentally modify the silder when I just want to scroll (which is stupid)
	// case SDL_MOUSEWHEEL:
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		if(slider_held)
		{
			if((e.motion.state & SDL_BUTTON_LMASK) == 0 && (e.motion.state & SDL_BUTTON_RMASK) == 0)
			{
				slider_held = false;
			}
			else
			{
				internal_move_to(mouse_x);
				// VALUE HAS CHANGED
				return true;
			}
		}
		// helps unfocus other elements.
		if(internal_slider_inside(mouse_x, mouse_y))
		{
			// eat
			set_mouse_event_clipped(e);
			return false;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			float mouse_x = static_cast<float>(e.button.x);
			// float mouse_y = static_cast<float>(e.button.y);
			if(slider_held)
			{
				internal_move_to(mouse_x);

				unfocus();
				// eat
				set_event_unfocus(e);
				// VALUE HAS CHANGED
				return true;
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
		if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
		{
			if(is_mouse_event_clipped(e))
			{
				unfocus();
				break;
			}
			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			if(internal_slider_inside(mouse_x, mouse_y))
			{
				slider_held = true;
				// eat
				set_event_unfocus(e);
				return false;
			}
			// snap the slider to the location clicked.
			if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
			   box_xmin <= mouse_x)
			{
				slider_thumb_click_offset = box_xmin + slider_thumb_size / 2;
				internal_move_to(mouse_x);
				slider_held = true;
				// eat
				set_event_unfocus(e);
				// VALUE HAS CHANGED
				return true;
			}
			unfocus();
		}
		break;
	}
	return false;
}

void mono_normalized_slider_object::draw_buffer()
{
	ASSERT(font_painter != NULL);

	update_buffer = false;

	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	// draw the scrollbar bbox
	{
		float xmin = box_xmin;
		float xmax = box_xmax;
		float ymin = box_ymin;
		float ymax = box_ymax;

		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}
	// draw the scrollbar thumb
	{
		float thumb_offset = static_cast<float>(get_slider_normalized()) *
							 ((box_xmax - box_xmin) - slider_thumb_size);

		float xmin = box_xmin + thumb_offset;
		float xmax = box_xmin + thumb_offset + slider_thumb_size;
		float ymin = box_ymin;
		float ymax = box_ymax;

		batcher->draw_rect({xmin, ymin, xmax, ymax}, white_uv, scrollbar_color);
		batcher->draw_rect({xmin, ymin, xmin + 1, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymin, xmax, ymin + 1}, white_uv, bbox_color);
		batcher->draw_rect({xmax - 1, ymin, xmax, ymax}, white_uv, bbox_color);
		batcher->draw_rect({xmin, ymax - 1, xmax, ymax}, white_uv, bbox_color);
	}
	update_buffer = false;
}
void mono_normalized_slider_object::unfocus()
{
	slider_held = false;
	slider_thumb_click_offset = -1;
}
void mono_normalized_slider_object::resize_view(float xmin, float xmax, float ymin, float ymax)
{
	box_xmin = std::floor(xmin);
	box_xmax = std::floor(xmax);
	box_ymin = std::floor(ymin);
	box_ymax = std::floor(ymax);
	update_buffer = true;
}
bool mono_normalized_slider_object::internal_slider_inside(float mouse_x, float mouse_y)
{
	float thumb_offset =
		static_cast<float>(get_slider_normalized()) * ((box_xmax - box_xmin) - slider_thumb_size);
	float xmin = box_xmin + thumb_offset;
	float xmax = box_xmin + thumb_offset + slider_thumb_size;
	float ymin = box_ymin;
	float ymax = box_ymax;

	if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
	{
		slider_thumb_click_offset = mouse_x - thumb_offset;
		return true;
	}

	return false;
}
void mono_normalized_slider_object::internal_move_to(float mouse_x)
{
	// NOTE: I am not sure if float -> double is bad, I know it is, but how bad is it?
	slider_value =
		(mouse_x - slider_thumb_click_offset) / ((box_xmax - box_xmin) - slider_thumb_size);

	// the value is currently 0-1, do linear interpolation to find the value.
	slider_value = (slider_min * (1 - slider_value) + slider_max * slider_value);

	// clamp
	slider_value = std::max(slider_min, std::min(slider_max, slider_value));

	update_buffer = true;
}
