#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS
#else
#error Define your Operating System
#endif

//NOTE(fran): less annoying OS dependent call masking in user code
#ifdef OS_WINDOWS
#define IF_OS_WINDOWS(...) __VA_ARGS__
#else
#define IF_OS_WINDOWS(...) 
#endif

namespace OS { //Custom types common to all OSes
	enum window_creation_flags : u64 {
		topmost = 1 << 0,
		notaskbar = 1 << 1, //Window never shows up in the taskbar
		resizeborder = 1 << 2, //Window can be resized by clicking & dragging its borders
		clickthrough = 1 << 3, //All input is passed through the window to windows below it

		//os_managed = 1 << 63, //The 'non client' area of the window is completely managed by the OS

		//NOTE(fran): combinations of commonly used flags
		contextmenu = topmost | notaskbar, //TODO(fran): this should probably be defined per OS
	};
	//constexpr window_creation_flags contextmenu = (window_creation_flags)(topmost | notaskbar);

	enum class cursor_style {
		arrow = 0, text, hand
	};

}

#ifdef OS_WINDOWS
#include "OS_windows.h"
#else
#error Define your OS's implementation
#endif

//TODO(fran): it's annoying that currently the function declaration must exist on _all_ OS implementations for that OS's build to work even if it does nothing on that OS
	//The only current solution is manually #defining the function call out at the call site, which is not great
	//We need a way to define stubs
	//TODO(fran): look at using #pragma weak