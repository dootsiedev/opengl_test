#include "global.h"

#include "ui.h"

#include "app.h"

BUTTON_RESULT mono_button_object::input(SDL_Event& e)
{
	if(disabled)
	{
		return BUTTON_RESULT::CONTINUE;
	}
	switch(e.type)
	{
	case SDL_MOUSEMOTION: {
		float mouse_x = static_cast<float>(e.motion.x);
		float mouse_y = static_cast<float>(e.motion.y);

		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];

		hover_over = ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x;
	}
	break;
	case SDL_MOUSEBUTTONDOWN: {
		float mouse_x = static_cast<float>(e.button.x);
		float mouse_y = static_cast<float>(e.button.y);

		float xmin = button_rect[0];
		float xmax = button_rect[0] + button_rect[2];
		float ymin = button_rect[1];
		float ymax = button_rect[1] + button_rect[3];

		if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
		{
			if(mouse_button_down)
			{
				// slog("click\n");
				return BUTTON_RESULT::TRIGGER;
			}
			clicked_on = true;
		}
		else
		{
			clicked_on = false;
		}
	}
	break;
	case SDL_MOUSEBUTTONUP: {
		if(!mouse_button_down && clicked_on)
		{
            clicked_on = false;

			float mouse_x = static_cast<float>(e.button.x);
			float mouse_y = static_cast<float>(e.button.y);

			float xmin = button_rect[0];
			float xmax = button_rect[0] + button_rect[2];
			float ymin = button_rect[1];
			float ymax = button_rect[1] + button_rect[3];

			if(ymax >= mouse_y && ymin <= mouse_y && xmax >= mouse_x && xmin <= mouse_x)
			{
				// slog("click\n");
				return BUTTON_RESULT::TRIGGER;
			}
		}
	}
	break;
	}

	return BUTTON_RESULT::CONTINUE;
}
bool mono_button_object::update(double delta_sec)
{
    // NOTE: I wouldn't need this if I used a setter for disabling the button...
	hover_over = !disabled && hover_over;

	// add fade
	fade += static_cast<float>(hover_over ? delta_sec : -delta_sec) * color_state.fade_speed;

	// clamp
	fade = std::min(fade, 1.f);
	fade = std::max(fade, 0.f);

	return true;
}
bool mono_button_object::draw_buffer()
{
	mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

	// normalize the colors 0-1
	float hot_fill[4] = {
		static_cast<float>(color_state.hot_fill_color[0]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[1]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[2]) / 255.f,
		static_cast<float>(color_state.hot_fill_color[3]) / 255.f};
	float idle_fill[4] = {
		static_cast<float>(color_state.idle_fill_color[0]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[1]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[2]) / 255.f,
		static_cast<float>(color_state.idle_fill_color[3]) / 255.f};

	// blend the colors.
	std::array<uint8_t, 4> fill_color = {
		static_cast<uint8_t>((hot_fill[0] * fade + idle_fill[0] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[1] * fade + idle_fill[1] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[2] * fade + idle_fill[2] * (1.f - fade)) * 255.f),
		static_cast<uint8_t>((hot_fill[3] * fade + idle_fill[3] * (1.f - fade)) * 255.f),
	};

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

	if(color_state.show_outline)
	{
		// outline
		font_painter->set_style(FONT_STYLE_OUTLINE);
		font_painter->set_color(color_state.text_outline_color);
		font_painter->set_anchor(TEXT_ANCHOR::CENTER_PERFECT);
		font_painter->set_xy(
			button_rect[0] + (button_rect[2] / 2.f), button_rect[1] + (button_rect[3] / 2.f));
		if(!font_painter->draw_text(text.c_str(), text.size()))
		{
			return false;
		}
	}

	font_painter->set_style(FONT_STYLE_NORMAL);
	font_painter->set_color(disabled ? color_state.disabled_text_color : color_state.text_color);
	font_painter->set_xy(
		button_rect[0] + (button_rect[2] / 2.f), button_rect[1] + (button_rect[3] / 2.f));
	if(!font_painter->draw_text(text.c_str(), text.size()))
	{
		return false;
	}

	font_painter->end();

    // there are not much gl calls here, but the text does modify the atlas.
	return GL_RUNTIME(__func__) == GL_NO_ERROR;
}
void mono_button_object::unfocus()
{
    clicked_on = false;
	hover_over = false;
}

//
// scrollable
//


bool mono_y_scrollable_area::input(SDL_Event& e)
{
    ASSERT(font_painter != NULL);

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
							font_painter->state.font->get_lineskip();
				// clamp
				scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
			}
		}
		break;
		case SDL_MOUSEMOTION: {
			float mouse_y = static_cast<float>(e.motion.y);
			if(y_scrollbar_held)
			{
				internal_scroll_y_to(mouse_y);
			}
		}
		break;
		case SDL_MOUSEBUTTONUP:
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);
				if(y_scrollbar_held)
				{
					internal_scroll_y_to(mouse_y);
					y_scrollbar_held = false;
					scroll_thumb_click_offset = -1;
					return true;
				}

				// helps unfocus other elements.
				if(box_ymax >= mouse_y && box_ymin <= mouse_y && box_xmax >= mouse_x &&
				   box_xmin <= mouse_x)
				{
					return true;
				}
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
			if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT)
			{
				float mouse_x = static_cast<float>(e.button.x);
				float mouse_y = static_cast<float>(e.button.y);

				if(internal_scroll_y_inside(mouse_x, mouse_y))
				{
					y_scrollbar_held = true;
					return true;
				}

				y_scrollbar_held = false;
				scroll_thumb_click_offset = -1;
				// unfocus();
			}
			break;
		}
	}
    return false;
}

void mono_y_scrollable_area::draw_buffer()
{
    ASSERT(font_painter != NULL);
    mono_2d_batcher* batcher = font_painter->state.batcher;
	auto white_uv = font_painter->state.font->get_font_atlas()->white_uv;

    // TODO: draw_bbox
    

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
}

bool mono_y_scrollable_area::internal_scroll_y_inside(float mouse_x, float mouse_y)
{
	float scrollbar_max_height = (box_ymax - box_ymin);

	float thumb_height = scrollbar_max_height * ((box_ymax - box_ymin) / content_h);
	thumb_height = std::max(thumb_height, scrollbar_thumb_min_size);

	float scroll_ratio = (scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
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

	float scroll_ratio = (scrollbar_max_height - thumb_height) / (content_h - (box_ymax - box_ymin));
	scroll_y = (mouse_y - scroll_thumb_click_offset) / scroll_ratio;

	// clamp
	scroll_y = std::max(0.f, std::min(content_h - (box_ymax - box_ymin), scroll_y));
}
