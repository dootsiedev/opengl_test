#include "../global.h"
#include "options.h"

bool options_state::init(
    font_style_interface* font_, mono_2d_batcher* batcher_, shader_mono_state& mono_shader)
{
    return keybinds.init(font_, batcher_, mono_shader);
}

bool options_state::destroy()
{
    return keybinds.destroy();
}

OPTIONS_RESULT options_state::input(SDL_Event& e)
{
    switch(keybinds.input(e))
    {
		case OPTIONS_KEYBINDS_RESULT::CONTINUE: return OPTIONS_RESULT::CONTINUE;
		case OPTIONS_KEYBINDS_RESULT::EAT: return OPTIONS_RESULT::EAT;
		case OPTIONS_KEYBINDS_RESULT::CLOSE: return OPTIONS_RESULT::CLOSE;
		case OPTIONS_KEYBINDS_RESULT::ERROR: return OPTIONS_RESULT::ERROR;
    }
    return OPTIONS_RESULT::CONTINUE;
}

bool options_state::update(double delta_sec)
{
    return keybinds.update(delta_sec);
}

// this requires the atlas texture to be bound with 1 byte packing
bool options_state::render()
{
    return keybinds.render();
}

void options_state::resize_view()
{
    keybinds.resize_view();
}

// call this when you need to unfocus, like for example if you press escape or something.
void options_state::unfocus()
{
    keybinds.unfocus();
}