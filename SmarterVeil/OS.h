#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS
#else
#error Define your Operating System
#endif

namespace OS { //Custom types common to all OSes
	enum window_creation_flags : u64 {
		topmost = 1 << 0,
		notaskbar = 1 << 1, //Window never shows up in the taskbar
		resizeborder = 1 << 2, //Window can be resized by clicking & dragging its borders
		clickthrough = 1 << 3 //All input is passed through the window to windows below it
	};
	constexpr window_creation_flags contextmenu = (window_creation_flags)(topmost | notaskbar);
}

#ifdef OS_WINDOWS
#include "OS_windows.h"
#else
#error Define your OS's implementation
#endif