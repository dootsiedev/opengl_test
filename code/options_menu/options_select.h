#pragma once

#include "../global.h"

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

};