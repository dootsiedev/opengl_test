#pragma once

#include "../global.h"

#include "../ui.h"

enum class OPTIONS_SELECT_RESULT
{
	EAT,
	OPEN_VIDEO,
	OPEN_CONTROLS,
#ifdef AUDIO_SUPPORT
	OPEN_AUDIO,
#endif
	CLOSE,
	CONTINUE,
	ERROR
};

struct options_select_state
{
    // this is just a list of buttons, I will just associate the button with the enum.
    struct select_entry{
        mono_button_object button;
        OPTIONS_SELECT_RESULT result;
    };
    std::vector<select_entry> select_entries;
};