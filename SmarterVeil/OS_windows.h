#pragma once

#include "win32_sdk.h"
#include "resource.h"
#include <shlobj_core.h> // SHGetKnownFolderPath
#include <shellapi.h> // NOTIFYICONDATA (everything related to the tray icon)
#include <timeapi.h> //TIMECAPS, timeBeginPeriod

#pragma comment (lib, "winmm") // timeapi.h

#define WM_GETTOPMARGIN (WM_USER+1)

#define WM_TRAY (WM_USER + 3)

#define WM_CUSTOMSETCURSOR (WM_USER + 4) /*wParam=HCURSOR*/

//NOTE(fran): custom window messages can go from WM_USER(0x0400) up to 0x0FFF

//TODO?(fran): Windows API calls that need chaging for Dpi aware versions:
	// Reference: https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
	// Single DPI version	Per - Monitor version
	// GetSystemMetrics	    GetSystemMetricsForDpi
	// AdjustWindowRectEx	AdjustWindowRectExForDpi
	// SystemParametersInfo	SystemParametersInfoForDpi
	// GetDpiForMonitor	    GetDpiForWindow


namespace OS {

struct window_handle { //TODO(fran): are we happy with having to define this before including Iu?
	HWND hwnd;
	b32 operator!() { return !this->hwnd; } //TODO(fran): replace with is_valid?
	b32 operator==(window_handle cmp_wnd) { return this->hwnd == cmp_wnd.hwnd; }
};

}

#include "iu_declaration.h"

namespace _OS { //NOTE(fran): helpers for the OS itself, should not be used by outside callers

internal rc2 RECT_to_rc2(RECT r)
{
	rc2 res = { .x = (f32)r.left, .y = (f32)r.top, .w = (f32)RECTW(r), .h = (f32)RECTH(r), };
	return res;
}

internal RECT rc2_to_RECT(rc2 r)
{
	RECT res = { .left = (i32)r.left, .top = (i32)r.top, .right = (i32)r.right(), .bottom = (i32)r.bottom(), };
	return res;
}

//NOTE(fran): small memory would be anything that will be generally below the OS's page size
internal void* alloc_small_mem(u64 sz)
{
	return malloc(sz);
}

internal void free_small_mem(void* mem)
{
	free(mem);
}

internal s16 convert_to_s16(const s8 s)
{
	//IMPORTANT(fran): unfortunately on Windows most functions expect null terminated strings, therefore this function will insert the null terminator if not present
	s16 res;
	i64 sz_char;

	b32 null_terminate = s.chars && s.cnt && (s.chars[s.cnt - 1] != 0);

	constexpr b32 use_estimate = true;
	if constexpr (use_estimate)
		sz_char = (s.sz() * 2 * 4) / sizeof(*res.chars);
	else
		sz_char = MultiByteToWideChar(CP_UTF8, 0, (char*)s.chars, s.sz(), 0, 0);//NOTE: pre windows vista this may need a change

	if (null_terminate) sz_char += 1;

	size_t sz_bytes = sz_char * sizeof(*res.chars);

	res.chars = (decltype(res.chars))alloc_small_mem(sz_bytes);
	res.cnt_allocd = sz_char;
	res.cnt = MultiByteToWideChar(CP_UTF8, 0, (char*)s.chars, s.sz(), (wchar_t*)res.chars, sz_char);

	assert(res.cnt || (GetLastError() != ERROR_INSUFFICIENT_BUFFER));

	if (null_terminate) res.chars[res.cnt] = 0; //NOTE(fran): the null terminator will exist but we wont update the cnt, the Windows code will have that knowledge and handle s16 corretly when it has to

	return res;
}

internal s8 convert_to_s8(const s16 s)
{
	s8 res;
	i64 sz_char;

	constexpr b32 use_estimate = true;
	if constexpr (use_estimate)
		sz_char = (s.sz() + s.sz() / 2 + (s.sz() ? sizeof(*res.chars) : 0)) / sizeof(*res.chars);
	else
		sz_char = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.chars, s.cnt, 0, 0, 0, 0);//TODO(fran): pre windows vista this may need a change

	u64 sz_bytes = sz_char * sizeof(*res.chars);

	res.chars = (decltype(res.chars))alloc_small_mem(sz_bytes);
	res.cnt_allocd = sz_char;
	res.cnt = WideCharToMultiByte(CP_UTF8, 0, (wchar_t*)s.chars, s.cnt, (char*)res.chars, sz_char, 0, 0) / sizeof(*res.chars);

	assert(res.cnt || (GetLastError() != ERROR_INSUFFICIENT_BUFFER));

	return res;
}

internal u32 _GetRefreshRateHz(HWND wnd) //TODO(fran): I had to put this code here instead of in OS because the Tray functions need to know the refresh rate in order to do the minimize/restore animation
{
	//TODO(fran): this may be simpler with GetDeviceCaps
	u32 res = 60;
	HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
	if (mon) {
		MONITORINFOEX nfo;
		nfo.cbSize = sizeof(nfo);
		if (GetMonitorInfo(mon, &nfo)) {
			DEVMODE devmode;
			devmode.dmSize = sizeof(devmode);
			devmode.dmDriverExtra = 0;
			if (EnumDisplaySettings(nfo.szDevice, ENUM_CURRENT_SETTINGS, &devmode)) {
				res = devmode.dmDisplayFrequency;
			}
		}
	}
	return res;
}

internal b32 windows_directory_exists(const s16 directory_path)
{
	//Reference: https://stackoverflow.com/questions/3828835/how-can-we-check-if-a-file-exists-or-not-using-win32-program
	//NOTE(fran): Raymond Chen approved: https://devblogs.microsoft.com/oldnewthing/20071023-00/?p=24713
	b32 res;
	DWORD attribs = GetFileAttributesW((wchar_t*)directory_path.chars);
	res = (attribs != INVALID_FILE_ATTRIBUTES) && (attribs & FILE_ATTRIBUTE_DIRECTORY);
	return res;
}

//Creates all directories before the last slash (\\ or /) if they dont exist
internal void windows_create_subdirectories(const s16 filename)
{
	//NOTE(fran): Windows does _not_ by default create unexisting directories when creating a file

	auto find_next_slash = [](const s16 filename, u64 from) -> u64
	{
		auto a = filename.find_next(u'\\', from);
		auto b = filename.find_next(u'/', from);
		return (a < b) ? a : b;
	};
	auto find_prev_slash = [](const s16 filename, u64 from) -> u64
	{
		auto a = filename.find_previous(u'\\', from);
		auto b = filename.find_previous(u'/', from);
		return (a > b && a != U64MAX) ? a : b;
	};

	s16 dir = filename;
	u64 next_dir = 0;
	u64 last_dir = find_prev_slash(filename, filename.cnt - 1);
	while ((next_dir = find_next_slash(filename, next_dir)) != U64MAX)
	{
		auto c = dir[next_dir];
		dir[next_dir] = 0;
		b32 res = CreateDirectoryW((wchar_t*)dir.chars, 0);
		//assert(res || (GetLastError() == ERROR_ALREADY_EXISTS));
		dir[next_dir] = c;
		next_dir++;
		if (next_dir > last_dir) break;
	}

#ifdef DEBUG_BUILD
	auto c = dir[last_dir];
	dir[last_dir] = 0;
	assert(windows_directory_exists(dir));
	dir[last_dir] = c;
#endif
}

internal s8 get_work_folder() {
	constexpr utf8 app_folder[] = appname u8"/";

	s8 res;
	wchar_t* general_folder; defer{ CoTaskMemFree(general_folder); };
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &general_folder); //TODO(fran): only works on Vista and above (for XP and lower use SHGetFolderPathW)
	u64 general_folder_cnt = s_cnt(general_folder);
	u64 general_folder_sz = general_folder_cnt * sizeof(*general_folder);

	s8 general_folder8 = convert_to_s8({ .chars = (utf16*)general_folder, .cnt = general_folder_cnt, .cnt_allocd = general_folder_cnt });
	defer{ free_small_mem(general_folder8.chars); };

	u64 work_folder_cnt = general_folder8.cnt + ArrayCount(app_folder);
	u64 work_folder_sz = work_folder_cnt * sizeof(res[0]);

	res =
	{
		.chars = (decltype(&res[0]))alloc_small_mem(work_folder_sz),
		.cnt = 0,
		.cnt_allocd = work_folder_cnt,
	};

	res += general_folder8; //TODO(fran): we can save ourselves this copy if we alloc more space for general_folder8 and write to there
	res += u8"/"; //TODO(fran): how well does & did Windows handle mixed \ and / paths?
	res += app_folder;
	return res;
}

//TODO(fran): for this OS internal kinds of things we should have the string type change with OS
internal b32 windows_set_working_directory(s16 dir) 
{
	b32 res = SetCurrentDirectoryW((wchar_t*)dir.chars);
	return res;
}

#include "win32_tray.h"

internal bool test_pt_rc(POINT p, RECT r) 
{
	bool res = p.y >= r.top && p.y < r.bottom && p.x >= r.left && p.x < r.right;
	return res;
}

internal iu::ui_key VkToIuKey(u8 vk) //TODO(fran): should this function also know whether it is an extended key?
{
	switch (vk)
	{
	case VK_ESCAPE: return iu::ui_key::Esc;
	case VK_F1:	return iu::ui_key::F1;
	case VK_F2:	return iu::ui_key::F2;
	case VK_F3:	return iu::ui_key::F3;
	case VK_F4:	return iu::ui_key::F4;
	case VK_F5:	return iu::ui_key::F5;
	case VK_F6:	return iu::ui_key::F6;
	case VK_F7:	return iu::ui_key::F7;
	case VK_F8:	return iu::ui_key::F8;
	case VK_F9:	return iu::ui_key::F9;
	case VK_F10: return iu::ui_key::F10;
	case VK_F11: return iu::ui_key::F11;
	case VK_F12: return iu::ui_key::F12;
	case VK_F13: return iu::ui_key::F13;
	case VK_F14: return iu::ui_key::F14;
	case VK_F15: return iu::ui_key::F15;
	case VK_F16: return iu::ui_key::F16;
	case VK_F17: return iu::ui_key::F17;
	case VK_F18: return iu::ui_key::F18;
	case VK_F19: return iu::ui_key::F19;
	case VK_F20: return iu::ui_key::F20;
	case VK_F21: return iu::ui_key::F21;
	case VK_F22: return iu::ui_key::F22;
	case VK_F23: return iu::ui_key::F23;
	case VK_F24: return iu::ui_key::F24;
	case 'A': return iu::ui_key::A;
	case 'B': return iu::ui_key::B;
	case 'C': return iu::ui_key::C;
	case 'D': return iu::ui_key::D;
	case 'E': return iu::ui_key::E;
	case 'F': return iu::ui_key::F;
	case 'G': return iu::ui_key::G;
	case 'H': return iu::ui_key::H;
	case 'I': return iu::ui_key::I;
	case 'J': return iu::ui_key::J;
	case 'K': return iu::ui_key::K;
	case 'L': return iu::ui_key::L;
	case 'M': return iu::ui_key::M;
	case 'N': return iu::ui_key::N;
	case 'O': return iu::ui_key::O;
	case 'P': return iu::ui_key::P;
	case 'Q': return iu::ui_key::Q;
	case 'R': return iu::ui_key::R;
	case 'S': return iu::ui_key::S;
	case 'T': return iu::ui_key::T;
	case 'U': return iu::ui_key::U;
	case 'V': return iu::ui_key::V;
	case 'W': return iu::ui_key::W;
	case 'X': return iu::ui_key::X;
	case 'Y': return iu::ui_key::Y;
	case 'Z': return iu::ui_key::Z;
	case '0': return iu::ui_key::_0;
	case '1': return iu::ui_key::_1;
	case '2': return iu::ui_key::_2;
	case '3': return iu::ui_key::_3;
	case '4': return iu::ui_key::_4;
	case '5': return iu::ui_key::_5;
	case '6': return iu::ui_key::_6;
	case '7': return iu::ui_key::_7;
	case '8': return iu::ui_key::_8;
	case '9': return iu::ui_key::_9;
	case VK_NUMPAD0: return iu::ui_key::Numpad0;
	case VK_NUMPAD1: return iu::ui_key::Numpad1;
	case VK_NUMPAD2: return iu::ui_key::Numpad2;
	case VK_NUMPAD3: return iu::ui_key::Numpad3;
	case VK_NUMPAD4: return iu::ui_key::Numpad4;
	case VK_NUMPAD5: return iu::ui_key::Numpad5;
	case VK_NUMPAD6: return iu::ui_key::Numpad6;
	case VK_NUMPAD7: return iu::ui_key::Numpad7;
	case VK_NUMPAD8: return iu::ui_key::Numpad8;
	case VK_NUMPAD9: return iu::ui_key::Numpad9;
	case VK_MULTIPLY: return iu::ui_key::NumpadMultiply;
	case VK_ADD: return iu::ui_key::NumpadAdd;
	case VK_SUBTRACT: return iu::ui_key::NumpadSubtract;
	case VK_DECIMAL: return iu::ui_key::NumpadDecimal;
	case VK_DIVIDE: return iu::ui_key::NumpadDivide;
		//	return iu::ui_key::NumpadEnter; //TODO(fran): this is VK_RETURN + the extended key flag
	case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return iu::ui_key::Ctrl;
	case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return iu::ui_key::Shift;
	case VK_MENU: case VK_LMENU: case VK_RMENU: return iu::ui_key::Alt;
	case VK_LWIN: case VK_RWIN: return iu::ui_key::OS;
	case VK_TAB: return iu::ui_key::Tab;
	case VK_CAPITAL: return iu::ui_key::CapsLock;
	case VK_NUMLOCK: return iu::ui_key::NumLock;
	case VK_SCROLL: return iu::ui_key::ScrollLock;
	case VK_UP: return iu::ui_key::UpArrow;
	case VK_DOWN: return iu::ui_key::DownArrow;
	case VK_LEFT: return iu::ui_key::LeftArrow;
	case VK_RIGHT: return iu::ui_key::RightArrow;
	case VK_OEM_7: return iu::ui_key::Apostrophe;
	case VK_OEM_COMMA: return iu::ui_key::Comma;
	case VK_OEM_PERIOD: return iu::ui_key::Period;
	case VK_OEM_1: return iu::ui_key::Semicolon;
	case VK_OEM_MINUS: return iu::ui_key::Minus;
	case VK_OEM_PLUS: return iu::ui_key::Equal; //TODO(fran): wtf, on US it is =, on Spanish it is +
	case VK_OEM_4: return iu::ui_key::LeftBracket;
	case VK_OEM_6: return iu::ui_key::RightBracket;
	case VK_OEM_5: return iu::ui_key::Backslash;
	case VK_OEM_2: return iu::ui_key::Forwardslash;
	case VK_OEM_3: return iu::ui_key::GraveAccent;
	case VK_OEM_102: return iu::ui_key::LessThan;
	case VK_PRIOR: return iu::ui_key::PageUp;
	case VK_NEXT: return iu::ui_key::PageDown;
	case VK_HOME: return iu::ui_key::Home;
	case VK_END:  return iu::ui_key::End;
	case VK_INSERT: return iu::ui_key::Insert;
	case VK_DELETE: return iu::ui_key::DeleteForward;
	case VK_BACK: return iu::ui_key::DeleteBackward;
	case VK_SPACE: return iu::ui_key::Space;
	case VK_RETURN: return iu::ui_key::Enter;
	case VK_SNAPSHOT: return iu::ui_key::PrintScreen;
	case VK_APPS: return iu::ui_key::ContextMenu;
	case VK_VOLUME_UP: return iu::ui_key::VolumeUp;
	case VK_VOLUME_DOWN: return iu::ui_key::VolumeDown;
	case VK_VOLUME_MUTE: return iu::ui_key::VolumeMute;
	case VK_MEDIA_NEXT_TRACK: return iu::ui_key::MediaNextTrack;
	case VK_MEDIA_PREV_TRACK: return iu::ui_key::MediaPrevTrack;
	case VK_MEDIA_PLAY_PAUSE: return iu::ui_key::MediaPlayPause;
	case VK_MEDIA_STOP: return iu::ui_key::MediaStop;
	default:
	{
		OutputDebugStringA("Win32 WARNING: unhandled virtual key: ");
		OutputDebugStringA((char*)VkToString(vk).chars);
		OutputDebugStringA("\n");
		return iu::ui_key::Shift; //TODO(fran): return null key? tell the caller not to use the key? leave it like this?
	}
	}
}

LRESULT CALLBACK UIProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	auto MovWindow = [](HWND wnd, RECT rc) { MoveWindow(wnd, rc.left, rc.top, RECTW(rc), RECTH(rc), false); };
	//auto _msgName = msgToString(message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");

	//TODO(fran): do not allow the window to get smaller than a certain threshold, currently the user can make the window be almost 0 width & 0 height

	//TODO(fran): add functionality that Windows doesnt provide: double click on _any_ of the resizing borders causes a window resize in that direction up to the corresponding screen bound, eg. double click that generates HTLEFT moves the window rect's left component to the left of the monitor it's in

	//TODO(fran): always do OS::SendWindowToTop() when a topmost window gets SW_SHOW?

	struct ProcState {
		i32 top_margin;
	};

	auto get_state = [](HWND hwnd) -> ProcState* {
		ProcState* state = (ProcState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		return state;
	};
	auto set_state = [](HWND hwnd, ProcState* state) {
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
	};

	ProcState* state = get_state(hWnd);

	//if ((GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_POPUP) == 0/*non os_managed*/) goto default_handling; //NOTE(fran): apply this on a per-message basis
#if 0
	local_persistence u32 last_hittest;
	local_persistence POINT mouse_pivot; //in screen coordinates
	local_persistence RECT rect_pivot; //in screen coordinates
#endif
	switch (message)
	{
		//IMPORTANT(fran): on WM_NCLBUTTONDOWN:
		// Rant: when resizing the window with the mouse this a$hole sends WM_SYSCOMMAND with SC_MOVE which triggers the stupid hooking into your thread (and very politely notifies about it with wm_entersizemove), this is stupid, and probably just old code that no one ever got to fix. Probably they originally decided to hook your thread and call your wndproc straight up for performance concerns but it now means that you can either handle all non client functions yourself or create a different thread that wont get hooked by windows and simply resend any important messages to there
		// Problem: your execution loop gets stopped and a Windows hook takes over your thread and straight calls the wndprocs like this one, you cannot execute code and get completely and unavoidably locked until the user stops resizing the window attached to this wndproc, this means that the contents of your window get horribly stretched without updating
		// Solution: escape from Windows completely, run all your code in another thread. Only create the windows from the main thread so their wndprocs get linked to that one and that's the one that stalls
		// No, dont try it: do _not_ try to handle WM_NCLBUTTONDOWN by yourself, it's not hard (except for the autoresizing preview animation that happens when touching screen borders, which idk how to do, nor did I bother to) but it's pointless wasted work. If Windows, in all its wisdom decided the best way to handle window resizing was by taking over the user's thread then hats off to them, we're leaving to another thread.
		// Another solution: if anyone has another solution that does not require moving to another thread please let me know
#if 0 //NOTE(fran): attempt to handle window resizing
	case WM_NCLBUTTONDOWN:
	{
		if (wParam == HTLEFT || wParam == HTRIGHT || wParam == HTTOP || wParam == HTTOPLEFT || wParam == HTTOPRIGHT || wParam == HTBOTTOM || wParam == HTBOTTOMLEFT || wParam == HTBOTTOMRIGHT)
		{
			mouse_pivot = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			GetWindowRect(hWnd, &rect_pivot);
			last_hittest = wParam;
			SetCapture(hWnd);
			return 0;
		}
		else return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	case WM_MOUSEMOVE:
	{
		if (GetCapture() == hWnd)
		{
			POINT mouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			MapWindowPoints(hWnd, HWND_DESKTOP, &mouse, 1);
			switch (last_hittest)
			{
			case HTLEFT:
			case HTRIGHT:
			case HTTOP:
			case HTTOPLEFT:
			case HTTOPRIGHT:
			case HTBOTTOM:
			case HTBOTTOMLEFT:
			{
				crash();
			} break;
			case HTBOTTOMRIGHT:
			{
				SetWindowPos(hWnd, nil, 0, 0, RECTW(rect_pivot) + mouse.x - mouse_pivot.x, RECTH(rect_pivot) + mouse.y - mouse_pivot.y, SWP_NOREDRAW | SWP_NOMOVE | SWP_NOZORDER);
			} break;
			}
			return 0;
		}
		else return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	case WM_LBUTTONUP:
	{
		if (GetCapture() == hWnd)
			ReleaseCapture();
	} break;
#endif
	case WM_NCCREATE:
	{
		ProcState* st = (ProcState*)calloc(1, sizeof(ProcState));
		assert(st);
		set_state(hWnd, st);

		st->top_margin = 6;

		goto default_handling;
	} break;
	case WM_NCDESTROY:
	{
		free(state);
		set_state(hWnd, nil);

		goto default_handling;
	} break;
	case WM_NCLBUTTONDOWN:
	{
		ReleaseCapture(); //NOTE(fran): it seems like the Move operation (entersizemove) can _not_ start if the mouse is currently being captured by SetCapture()
		return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	case WM_NCCALCSIZE: //TODO(fran): for some magical reason the veil wnd can automatically work with WS_THICKBORDER without all this crap, but the border is transparent and exists outside the window on _all_ sides, which is not really preferable for the top border, but much less painful to program. Therefore at least find out what this window lacks in order to work too, for that continue with the UICOMPOSITOR macro (I feel like we need to set the D3DDevice with _premultiplied alpha_)
	{
		if ((BOOL)wParam) {
			auto res = DefWindowProcW(hWnd, message, wParam, lParam);
			NCCALCSIZE_PARAMS* calcsz = (NCCALCSIZE_PARAMS*)lParam;

			RECT emptyrc{ 0 };
			AdjustWindowRectEx(&emptyrc, GetWindowLongPtrW(hWnd, GWL_STYLE), false, 0);
			//TODO(fran): AdjustWindowRectExForDpi (or simply get the dpi and scale manually)
			state->top_margin = RECTH(emptyrc) / 2;

			RECT winrc; GetWindowRect(hWnd, &winrc);
			RECT clirc; GetClientRect(hWnd, &clirc);

			calcsz->rgrc[0].top -= state->top_margin;
			return res;
		}
		else return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	case WM_NCHITTEST:
	{
		auto res = DefWindowProcW(hWnd, message, wParam, lParam);
		if (res == HTCLIENT)
		{
			RECT caption_rc{ 0 };
			AdjustWindowRectEx(&caption_rc, WS_OVERLAPPEDWINDOW, false, 0);

			i32 top_side_margin = absolute_value(caption_rc.top);
			POINT mouse{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; ScreenToClient(hWnd, &mouse);

			RECT rc; GetClientRect(hWnd, &rc);
			RECT top_border = { 0, 0, RECTW(rc), state->top_margin };
			RECT topleft_border = top_border; topleft_border.right = top_side_margin;
			RECT topright_border = top_border; topright_border.left = RECTW(rc) - top_side_margin;

			if (_OS::test_pt_rc(mouse, topleft_border)) res = HTTOPLEFT;
			else if (_OS::test_pt_rc(mouse, topright_border)) res = HTTOPRIGHT;
			else if (_OS::test_pt_rc(mouse, top_border)) res = HTTOP;

			//TODO(fran): this solution also has another small problem, hittesting for buttons that overlap with the border, what chrome does for example is to give the buttons precedence over the border and provide a 1 pixel gap on top of the buttons where border hittesting will trigger. We cant really do any of that since there's no connection between windows' UI system and ours, therefore both inputs will happen
		}
		return res;
	} break;
	case WM_CLOSE:
	{
		//NOTE(fran): as always Windows sucks & this msg is piped straight to the wndproc bypassing the msg queue
		iu::PushEventClose({ hWnd });
	} break;
	case WM_GETTOPMARGIN:
	{
		return state->top_margin;
	} break;
	case WM_TRAY:
	{
		//NOTE(fran): Windows strikes again! the tray message is sent straight to the proc bypassing the message queue
		switch (LOWORD(lParam))
		{
			//NOTE(fran): for some reason the tray only sends WM_LBUTTONDOWN once the mouse has been released, so here WM_LBUTTONDOWN counts as WM_LBUTTONUP, same for right click
		case WM_LBUTTONDOWN:
		{
			iu::PushEventTray({ hWnd }, iu::ui_key::MouseLeft, iu::ui_key_state::unclicked);
		} break;
		case WM_RBUTTONDOWN:
		{
			iu::PushEventTray({ hWnd }, iu::ui_key::MouseRight, iu::ui_key_state::unclicked);
		} break;
		}
	} break;
	case WM_DPICHANGED:
	{
		//NOTE(fran): Uhh Windows, Windows... why do you do this to yourself? This msg bypasses the msg queue
		assert(LOWORD(wParam) == HIWORD(wParam)); //check X dpi equals Y dpi
		iu::PushEventDpi({ hWnd }, (f32)LOWORD(wParam) / 96.f /*aka USER_DEFAULT_SCREEN_DPI */);

		//TODO(fran): I find it really strange that DefWindowProcW does not do the window resizing for us, wtf? I have to do it manually
		RECT suggested_window = *(RECT*)lParam;
		MovWindow(hWnd, suggested_window); //TODO(fran): test the rcs we get, I've seen that for many applications this suggested new window is terrible, both in position & size, see if we can come up with a better suggestion
	} break;
	case WM_CUSTOMSETCURSOR: { SetCursor((HCURSOR)wParam); } break;
	default:
	{
		default_handling:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	}
	return 0;
}

internal void io_thread_handler()
{
	//TODO(fran): we could move all this msg processing code to the OS specific section and simply provide access to the iu's message queue
	while (true)
	{
		MSG msg;
		//NOTE(fran): under the hood PeekMessage is responsible for handling SendMessage (aka dispatching straight to the wndproc), the msgs sent through that way are hidden from the main msg queue & not available to the user, aka me, or at least I havent found a way to get to them yet before they are dispatched
		while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			OS::window_handle wnd{ msg.hwnd };

			if (!wnd)
			{
				//TODO(fran): find out if we care about any broadcast messages (msgs with a null hwnd), in that case we'd need to send that single event to iu all windows
				OutputDebugStringA("HWND NULL: "); OutputDebugStringA(msgToString(msg.message)); OutputDebugStringA("\n");
			}

			switch (msg.message)
			{
			case WM_LBUTTONDOWN:
			{
				SetCapture(msg.hwnd);
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseLeft, iu::ui_key_state::clicked);
			} break;
			case WM_LBUTTONDBLCLK:
			{
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseLeft, iu::ui_key_state::doubleclicked);
			} break;
			case WM_LBUTTONUP:
			{
				ReleaseCapture();
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseLeft, iu::ui_key_state::unclicked);
			} break;
			case WM_RBUTTONDOWN:
			{
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseRight, iu::ui_key_state::clicked);
			} break;
			case WM_RBUTTONDBLCLK:
			{
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseRight, iu::ui_key_state::doubleclicked);
			} break;
			case WM_RBUTTONUP:
			{
				ReleaseCapture();
				iu::PushEventMouseButton(wnd, iu::ui_key::MouseRight, iu::ui_key_state::unclicked);
			} break;
			case WM_MOUSEMOVE:
			{
				TRACKMOUSEEVENT tme{ .cbSize = sizeof(tme), .dwFlags = TME_LEAVE, .hwndTrack = msg.hwnd };
				::TrackMouseEvent(&tme); //TODO(fran): only ask for tracking if not already tracking

				POINT p{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
				POINT screenP = p;
				MapWindowPoints(msg.hwnd, HWND_DESKTOP, &screenP, 1);

				iu::PushEventMousePos(wnd, v2_from(p.x, p.y), v2_from(screenP.x, screenP.y));
			} break;
			case WM_MOUSELEAVE:
			{
				v2 mouseP =
#ifdef DEBUG_BUILD
				{ -1, -1 }; //NOTE(fran): for ease of debugging
#else
				{ F32MIN, F32MIN };
#endif
				iu::PushEventMousePos(wnd, mouseP, mouseP);
			} break;
			case WM_MOUSEWHEEL:
			{
				f32 units_to_scroll = (f32)GET_WHEEL_DELTA_WPARAM(msg.wParam) / (f32)WHEEL_DELTA;
				iu::PushEventMouseWheel(wnd, { 0, units_to_scroll });
				//NOTE(fran): vertical scrolling is positive when the user moves the finger upwards through the wheel, & negative when moving the finger downwards
			} break;
			case WM_MOUSEHWHEEL:
			{
				f32 units_to_scroll = (f32)GET_WHEEL_DELTA_WPARAM(msg.wParam) / (f32)WHEEL_DELTA;
				iu::PushEventMouseWheel(wnd, { units_to_scroll, 0 });
			} break;
			//TODO(fran): WM_SYSCHAR?: sent when a key is pressed while also pressing Alt, used for keyboard menubar navigation
			case WM_CHAR:
			{
				utf16 c16 = msg.wParam;
				//TODO(fran): in the lparam is the scancode & extended key bit, can we use them for hotkeys or smth?
				utf8 c[4];
				i32 cnt = WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)&c16, 1, (char*)c, ArrayCount(c), 0, 0) / sizeof(*c);
				assert(cnt);
				for (i32 i = 0; i < cnt; i++) iu::PushEventText(wnd, c[i]);
			} break;
			case WM_HOTKEY:
			{
				i32 id = msg.wParam;
				u16 mods = LOWORD(msg.lParam);
				u8 vk = HIWORD(msg.lParam);
				iu::PushEventSystemWideHotkey(wnd, id);
			} break;
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
			{
				u8 vk = msg.wParam;
				iu::PushEventKey(wnd, _OS::VkToIuKey(vk), iu::ui_key_state::clicked);
			} break;
			}
			DispatchMessageW(&msg);
			//TODO(fran): test the delay of the dispatch compared to just proccesing the msgs from here as we currently do, if it isnt too bad (<0.01ms) then join all the msg processing inside the wndproc
		}
		::MsgWaitForMultipleObjectsEx(0, nil, INFINITE, QS_ALLINPUT, 0 | MWMO_ALERTABLE | MWMO_INPUTAVAILABLE); //TODO(fran): MWMO_INPUTAVAILABLE
	}
}

struct io_thread_setup {
	HANDLE io_thread;
	io_thread_setup()
	{
		//TODO(fran): we can create & destroy a dummy window on both the main & ui threads so their msg queues are initilized and we can do AttachThreadInput
		this->io_thread = ::CreateThread(nil, 0, [](void*)->DWORD {io_thread_handler(); return 0; }, nil, 0, nil);
	}
	~io_thread_setup()
	{
		::TerminateThread(this->io_thread, 0); //TODO(fran): we may want to do termination from inside the thread, TerminateThread is not too safe

	}
} global_persistence thread_setup;

internal f32 GetScalingForMonitor(HMONITOR monitor) //TODO(fran): OS code
{
	//stolen from: https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_win32.cpp
	f32 res;
	UINT xdpi = 96, ydpi = 96;

	local_persistence HMODULE WinSHCore = LoadLibraryW(L"shcore.dll");
	if (WinSHCore)
	{
		typedef HRESULT STDAPICALLTYPE get_dpi_for_monitor(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*); //GetDpiForMonitor
		local_persistence get_dpi_for_monitor* GetDpiForMonitor = (get_dpi_for_monitor*)GetProcAddress(WinSHCore, "GetDpiForMonitor");
		if (GetDpiForMonitor)
		{
			GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
			assert(xdpi == ydpi);
			res = xdpi / 96.0f;
		}
		else goto last_resort;
	}
	else
	{
		//TODO(fran): see if there are better alternatives (specifically for WXP & W7)
	last_resort:
		HDC dc = GetDC(HWND_DESKTOP); defer{ ReleaseDC(HWND_DESKTOP, dc); };
		xdpi = GetDeviceCaps(dc, LOGPIXELSX);
		ydpi = GetDeviceCaps(dc, LOGPIXELSY);
		assert(xdpi == ydpi);
		res = xdpi / 96.0f;
	}
	return res;
}

}

//NOTE(fran): services provided by the OS to the application

//IMPORTANT: for filepaths use / instead of \\, since / should be supported by default in all OSs

namespace OS
{
	struct hotkey_data {
		u8 vk;    //Virtual Key
		u16 mods; //Hotkey modifiers: ctrl, alt, shift
		LPARAM translation_nfo; //Extra information needed by Windows when converting hotkey to text, specifically scancode + extended key flag

		b32 has_hotkey()
		{
			b32 res = this->vk || this->mods;
			return res;
		}

		b32 is_different(hotkey_data hotkey_to_compare)
		{
			b32 res = this->vk != hotkey_to_compare.vk || this->mods != hotkey_to_compare.mods;
			return res;
		}
	};

	struct _work_folder {
		s8 dir_str;

		_work_folder() 
		{
			dir_str = _OS::get_work_folder();

			s16 work_dir16 = _OS::convert_to_s16(dir_str); defer{ _OS::free_small_mem(work_dir16.chars); };
			_OS::windows_create_subdirectories(work_dir16);
			b32 res = _OS::windows_set_working_directory(work_dir16);
			assert(res);
		}

		~_work_folder() { _OS::free_small_mem(dir_str.chars); };
	}global_persistence work_folder;

	struct _set_sleep_resolution {
		u32 desired_scheduler_ms = 1;
		
		_set_sleep_resolution()
		{
			TIMECAPS timecap;
			if (timeGetDevCaps(&timecap, sizeof(timecap)) == MMSYSERR_NOERROR)
				this->desired_scheduler_ms = maximum(this->desired_scheduler_ms, timecap.wPeriodMin);
			//TODO(fran): if we really wanted to we can spinlock if our desired ms is not met
			auto ret = timeBeginPeriod(this->desired_scheduler_ms);
			assert(ret == TIMERR_NOERROR);
		}

		~_set_sleep_resolution()
		{
			auto ret = timeEndPeriod(this->desired_scheduler_ms);
			assert(ret == TIMERR_NOERROR);
		}

	}global_persistence set_sleep_resolution;

	internal bool write_entire_file(const s8 filename, void* memory, u64 mem_sz)
	{
		bool res = false;
		s16 windows_filename = _OS::convert_to_s16(filename); defer{ _OS::free_small_mem(windows_filename.chars); };

		//First write to a temporary file, so that we dont leave a corrupted file in case the operation fails
		s16 temp = windows_filename.create_copy(_OS::alloc_small_mem, 5);
		defer{ _OS::free_small_mem(temp.chars); };
		temp += u".tmp";
		temp[temp.cnt++] = 0; //null terminate again

		_OS::windows_create_subdirectories(windows_filename);

		HANDLE hFile = CreateFileW((wchar_t*)temp.chars, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		if (hFile != INVALID_HANDLE_VALUE) {
			defer{ CloseHandle(hFile); };
			if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0))
			{
				//SUCCESS
				res = mem_sz == bytes_written;
			}
			else {
				crash();
				//TODO(fran): log
			}
		}

		if (res) res = MoveFileW((wchar_t*)temp.chars, (wchar_t*)windows_filename.chars);
		else DeleteFileW((wchar_t*)temp.chars);

		return res;
	}

	internal void free_file_memory(void* memory)
	{ //for read_entire_file_res
		if (memory) VirtualFree(memory, 0, MEM_RELEASE);
	}

	struct read_entire_file_res { void* mem; u64 sz; };
	internal read_entire_file_res read_entire_file(const s8 filename)
	{
		read_entire_file_res res{ 0 };

		auto filename16 = _OS::convert_to_s16(filename); defer{ _OS::free_small_mem(filename16.chars); };

		HANDLE hFile = CreateFileW((wchar_t*)filename16.chars, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		if (hFile != INVALID_HANDLE_VALUE) {
			defer{ CloseHandle(hFile); };
			if (LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz)) {
				assert(sz.QuadPart <= U32MAX);
				u32 sz32 = sz.QuadPart;
				void* mem = VirtualAlloc(0, sz32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);//TOOD(fran): READONLY?
				if (mem) {
					if (DWORD bytes_read; ReadFile(hFile, mem, sz32, &bytes_read, 0) && sz32 == bytes_read) {
						//SUCCESS
						res.mem = mem;
						res.sz = sz32;
					}
					else {
						free_file_memory(mem);
					}
				}
			}
		}
		return res;
	}

#define locale_max_length (LOCALE_NAME_MAX_LENGTH*2) /*utf8 length*/
	//returns true if successful, if res->cnt is at least equal to locale_max_length the function will never fail
	internal b32 get_user_locale(s8* res)
	{
		assert(res->cnt >= locale_max_length);
		utf16 loc16[LOCALE_NAME_MAX_LENGTH];
		b32 ret = GetUserDefaultLocaleName((wchar_t*)loc16, ArrayCount(loc16));

		s8 loc = _OS::convert_to_s8(temp_s(loc16)); defer{ _OS::free_small_mem(loc.chars); };
		*res += loc;

		return ret;
	}

	//TODO(fran): #define max_hotkey_char_cnt 128 (or smth like that)
	internal void HotkeyToString(hotkey_data hk, s8* hotkey_str)
	{
		//NOTE(fran): the best possible string conversion comes from the lparam value, only if the string for the virtual key code cant be retrieved from the lparam we attempt to get it from vk

		b32 first_mod = true;
		if (hk.mods & MOD_CONTROL) {
			if (!first_mod) *hotkey_str += u8"+";
			*hotkey_str += u8"Ctrl";
			first_mod = false;
		}
		if (hk.mods & MOD_ALT) {
			if (!first_mod) *hotkey_str += u8"+";
			*hotkey_str += u8"Alt";
			first_mod = false;
		}
		if (hk.mods & MOD_SHIFT) {
			if (!first_mod) *hotkey_str += u8"+";
			*hotkey_str += u8"Shift";
			first_mod = false;
		}

		s8 vk_str = stack_s(decltype(vk_str), 128);

		wchar_t vk_str16[64];
		//TODO(fran): we maaay want to have this stored in the language file instead?
		int keynameres = GetKeyNameTextW((LONG)(hk.translation_nfo & (~(LPARAM)(1 << 25))), vk_str16, ArrayCount(vk_str16));
		//NOTE(fran): set bit 25 to 0 say we dont care about left or right keys (eg Left vs Right Ctrl)

		if (keynameres) {
			//TODO(fran): 
			// if keynameres > 2 && IS_HIGH_SURROGATE(vk_str[0]) and advance +2 & keynameres-1
			if (keynameres > 1) _wcslwr_s(vk_str16 + 1, keynameres); //TODO(fran): better lowercase functions, and work with locales

			s8 vk_str8 = _OS::convert_to_s8({ .chars = (utf16*)vk_str16, .cnt = (u64)keynameres }); defer{ _OS::free_small_mem(vk_str8.chars); }; //TODO(fran): convert in place, straight to vk_str

			vk_str += vk_str8;
		}
		else {
			const s8 stringed_vk = VkToString(hk.vk);
			vk_str += stringed_vk;
		}

		if (vk_str.cnt) {
			if (!first_mod) *hotkey_str += u8"+";
			*hotkey_str += vk_str;
		}
	}

	internal window_handle Window(window_creation_flags flags)
	{
		window_handle res{ 0 };

		HINSTANCE instance = GetModuleHandleW(nil);

		WNDCLASSEXW wc{ 0 };
		wc.cbSize = sizeof(wc);
		wc.style = CS_DBLCLKS;
		wc.lpfnWndProc = &_OS::UIProc;
		wc.hInstance = instance;
		i32 SmallIconX = GetSystemMetrics(SM_CXSMICON);//TODO(fran): GetSystemMetricsForDpi?
		i32 LargeIconX = GetSystemMetrics(SM_CXICON);
		local_persistence HICON logo_large = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, LargeIconX, LargeIconX, 0);
		local_persistence HICON logo_small = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, SmallIconX, SmallIconX, 0);
		wc.hIcon = logo_large;
		wc.hIconSm = logo_small;
		wc.hCursor = nil;// LoadCursorW(nil, IDC_ARROW); //NOTE(fran): do _not_ use the hinstance parameter
		wc.lpszClassName = appnameL L"_ui_class";

		if (RegisterClassExW(&wc) || (GetLastError() == ERROR_CLASS_ALREADY_EXISTS))
		{
#if 1
			u32 extended_flags = WS_EX_NOREDIRECTIONBITMAP;
#else //os_managed
			u32 extended_flags = 0;
#endif
#ifndef DEBUG_BUILD
			extended_flags |= (flags & topmost) ? WS_EX_TOPMOST : 0;
#endif
			extended_flags |= (flags & notaskbar) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
			extended_flags |= (flags & clickthrough) ? WS_EX_LAYERED | WS_EX_TRANSPARENT : 0;

#if 1
			u32 basic_flags = WS_POPUP;
#else //os_managed
			u32 basic_flags = WS_OVERLAPPEDWINDOW;
#endif
			basic_flags |= (flags & resizeborder) ? WS_THICKFRAME | WS_MAXIMIZEBOX : 0; //TODO(fran): separate WS_MAXIMIZEBOX as a different flag?

			//SUPER IMPORTANT NOTE(fran): WS_MAXIMIZEBOX is _required_ for the aero window auto-resizing when touching screen borders

			res.hwnd = CreateWindowExW(
				extended_flags,
				wc.lpszClassName,
				appnameL,
				basic_flags,
				0, 0, 0, 0,
				nil, nil, nil, nil);

			UpdateWindow(res.hwnd);
		}

		return res;
	}

	internal void MoveWindow(window_handle wnd, rc2 placement)
	{
		MoveWindow(wnd.hwnd, (int)placement.x, (int)placement.y, (int)placement.w, (int)placement.h, false);
	}

	//Puts the window above all other windows on the screen
	internal void SendWindowToTop(window_handle wnd)
	{
		SetWindowPos(wnd.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		//TODO(fran): see also HWND_NOTOPMOST and HWND_TOP, we may need to check the window's WS_EX_TOPMOST style to decide which top flag to use
	}

	//The window is immediately shown to the user, without any animation
	internal void ShowWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_SHOW);
	}

	//The window is immediately hidden from the user, without any animation
	internal void HideWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_HIDE);
	}

	internal void MinimizeWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_MINIMIZE);
		//SetForegroundWindow(nil);
		//SetFocus(nil);
	}

	internal void MaximizeWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_MAXIMIZE);
	}

	internal void RestoreWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_RESTORE);
	}

	// Minimizes a window and creates an animation to make it look like it goes to the tray
	internal void MinimizeWndToTray(window_handle wnd)
	{//Thanks to: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
		if (_OS::GetDoAnimateMinimize())
		{
			RECT rcFrom, rcTo;

			// Get the rect of the window. It is safe to use the rect of the whole
			// window - DrawAnimatedRects will only draw the caption
			GetWindowRect(wnd.hwnd, &rcFrom);
			_OS::GetTrayWndRect(&rcTo);

			_OS::WindowToFromTray(wnd.hwnd, { rcFrom.left,rcFrom.top }, { rcTo.left,rcTo.top }, 200, true);
		}
		else ShowWindow(wnd.hwnd, SW_HIDE);// Just hide the window
	}

	// Restores a window and makes it look like it comes out of the tray and makes it back to where it was before minimizing
	internal void RestoreWndFromTray(window_handle wnd)
	{//Thanks to: https://www.codeproject.com/Articles/735/Minimizing-windows-to-the-System-Tray
		if (_OS::GetDoAnimateMinimize())
		{
			// Get the rect of the tray and the window. Note that the window rect
			// is still valid even though the window is hidden
			RECT rcFrom, rcTo;
			_OS::GetTrayWndRect(&rcFrom);
			GetWindowRect(wnd.hwnd, &rcTo);

			// Get the system to draw our animation for us
			_OS::WindowToFromTray(wnd.hwnd, { rcFrom.left,rcFrom.top }, { rcTo.left,rcTo.top }, 200, false);
		}
		else {
			// Show the window, and make sure we're the foreground window
			ShowWindow(wnd.hwnd, SW_SHOW);
			SetActiveWindow(wnd.hwnd);
			SetForegroundWindow(wnd.hwnd);
		}
	}

	//The window is (in Windows parlor) in the restored state, can be seen on the screen by the user, though it may be occluded by other windows
	internal b32 IsWindowVisible(window_handle wnd)
	{
		//TODO(fran): not sure IsWindowVisible is completely foolproof
		b32 res = IsWindowVisible(wnd.hwnd);
		return res;
	}

	internal b32 IsWindowMaximized(window_handle wnd)
	{
		b32 res = IsZoomed(wnd.hwnd);
		return res;
	}

	internal f32 GetWindowTopMargin(window_handle wnd)
	{
		f32 res = (f32)(i32)SendMessage(wnd.hwnd, WM_GETTOPMARGIN, 0, 0);
		return res;
	}

	internal rc2 GetWindowRenderRc(window_handle wnd) //TODO(fran): better name
	{
		rc2 res;
		RECT clientrc; GetClientRect(wnd.hwnd, &clientrc);
		assert((clientrc.left == 0) && (clientrc.top == 0));
		res = _OS::RECT_to_rc2(clientrc);
		return res;
	}

	internal rc2 GetWindowScreenRc(window_handle wnd) //TODO(fran): maybe also define one other function to return the screen client rect (non client area _not_ included)?
	{
		rc2 res;
		RECT clientrc; GetWindowRect(wnd.hwnd, &clientrc);
		res = _OS::RECT_to_rc2(clientrc);
		return res;
	}

	//NOTE(fran): returns position & dimensions of the monitor where the mouse is currently at
	internal rc2 GetMouseMonitorRc()
	{
		rc2 res;
		POINT p; ::GetCursorPos(&p);
		HMONITOR mon = ::MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST);
		assert(mon);
		MONITORINFO mon_nfo{.cbSize=sizeof(mon_nfo)};
		::GetMonitorInfoA(mon, &mon_nfo);
		res = _OS::RECT_to_rc2(mon_nfo.rcWork);
		return res;
	}

	internal u32 GetRefreshRateHz(window_handle wnd)
	{
		return _OS::_GetRefreshRateHz(wnd.hwnd);
	}

	internal void SetDPIAware()
	{
		typedef BOOL WINAPI set_process_dpi_aware(void);
		typedef HRESULT STDAPICALLTYPE set_process_dpi_awareness(PROCESS_DPI_AWARENESS);
		typedef BOOL WINAPI set_process_dpi_awareness_context(DPI_AWARENESS_CONTEXT);

		HMODULE WinUser = LoadLibraryW(L"user32.dll");
		set_process_dpi_awareness_context* SetProcessDpiAwarenessContext = (set_process_dpi_awareness_context*)GetProcAddress(WinUser, "SetProcessDpiAwarenessContext");
		if (SetProcessDpiAwarenessContext)
		{
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
		}
		else
		{
			HMODULE WinSHCore = LoadLibraryW(L"SHCore.dll");
			if (WinSHCore)
			{
				set_process_dpi_awareness* SetProcessDpiAwareness = (set_process_dpi_awareness*)GetProcAddress(WinSHCore, "SetProcessDpiAwareness");

				if (SetProcessDpiAwareness)
				{
					auto ret = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
					if (FAILED(ret))
					{
						ret = SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
						if (FAILED(ret)) goto last_resort;
					}
				}
			}
			else {
			last_resort:
				set_process_dpi_aware* SetProcessDPIAware = (set_process_dpi_aware*)GetProcAddress(WinUser, "SetProcessDPIAware");
				if (SetProcessDPIAware)
				{
					SetProcessDPIAware();
				}
			}
		}
	}

	//TODO(fran): custom cursors, we'd want to use a CreateCursor(ui_image) function for example and SetCursor(OS::cursor_handle) or something like that
	internal void SetCursor(cursor_style cursor)
	{
		HCURSOR c;
		switch (cursor)
		{
			case cursor_style::text: { c = LoadCursorW(nil, IDC_IBEAM); } break;
			case cursor_style::hand: { c = LoadCursorW(nil, IDC_HAND); } break;
			default: { c = LoadCursorW(nil, IDC_ARROW);} break;
		}
		//NOTE(fran): the loaded cursor is handled by the OS, we dont need to free it
		::SetCursor(c);
	}

	//TODO(fran): HACK: it seems to me that only the main thread can set the cursor
	internal void _SetCursor(OS::window_handle wnd, cursor_style cursor)
	{
		HCURSOR c;
		switch (cursor)
		{
		case cursor_style::text: { c = LoadCursorW(nil, IDC_IBEAM); } break;
		case cursor_style::hand: { c = LoadCursorW(nil, IDC_HAND); } break;
		default: { c = LoadCursorW(nil, IDC_ARROW); } break;
		}
		//NOTE(fran): the loaded cursor is handled by the OS, we dont need to free it
		
		SendMessageW(wnd.hwnd, WM_CUSTOMSETCURSOR, (WPARAM)c, 0);
	}

	internal f32 GetScalingForWindow(OS::window_handle wnd) //TODO(fran): OS code
	{
		f32 res;

		typedef UINT WINAPI get_dpi_for_window(HWND); //GetDpiForWindow

		local_persistence HMODULE WinUser = LoadLibraryW(L"user32.dll");
		local_persistence get_dpi_for_window* GetDpiForWindow = (get_dpi_for_window*)GetProcAddress(WinUser, "GetDpiForWindow");
		if (GetDpiForWindow)
		{
			res = (f32)GetDpiForWindow(wnd.hwnd) / 96.0f;
			assert(res); //NOTE(fran): GetDpiForWindow returns 0 for an invalid hwnd
		}
		else
		{
			HMONITOR monitor = MonitorFromWindow(wnd.hwnd, MONITOR_DEFAULTTONEAREST);
			res = _OS::GetScalingForMonitor(monitor);
		}
		return res;
	}

	internal f32 GetScalingForSystem()
	{
		f32 res;
		//TODO(fran): GetDpiForSystem, GetSystemDpiForProcess
		HMONITOR monitor = MonitorFromWindow(HWND_DESKTOP, MONITOR_DEFAULTTOPRIMARY);
		res = _OS::GetScalingForMonitor(monitor);
		return res;
	}

	internal f32 GetScalingForRc(rc2 rc)
	{
		f32 res;
		RECT r = _OS::rc2_to_RECT(rc);
		HMONITOR monitor = MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST);
		res = _OS::GetScalingForMonitor(monitor);
		return res;
	}

	internal sz2 GetSystemFontMetrics()
	{
		//TODO(fran): Docs recommend using SystemParametersInfoA + SPI_GETNONCLIENTMETRICS
		sz2 res;
		HFONT font = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
		HDC dc = ::GetDC(HWND_DESKTOP); defer{ ::ReleaseDC(HWND_DESKTOP,dc); };
		TEXTMETRICW tm; ::GetTextMetricsW(dc, &tm);
		res.w = tm.tmAveCharWidth;
		res.h = tm.tmHeight;
		return res;
	}

	struct tray_icon {
		NOTIFYICONDATAW notification;
	};

	internal tray_icon CreateTrayIcon(window_handle wnd) //TODO(fran): get the image from the caller
	{
		//NOTE(fran): realistically one application always has at most one tray icon, we wont bother with handling multiple ones

		//TODO(fran): given that NOTIFYICONDATAW is huge (almost 1KB) we may wanna get a pointer to a tray_icon and directly modify the data avoiding the copy

		tray_icon res{};
		HINSTANCE instance = GetModuleHandleW(nil);
		i32 SmallIconX = GetSystemMetrics(SM_CXSMICON);//TODO(fran): GetSystemMetricsForDpi?
		HICON icon = (HICON)LoadImageW(instance, MAKEINTRESOURCE(ICO_LOGO), IMAGE_ICON, SmallIconX, SmallIconX, 0);
		assert(icon);

		//TODO(fran): this may not work on Windows XP: https://docs.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-notifyicondataa
		res.notification =
		{
			.cbSize = sizeof(res.notification),
			.hWnd = wnd.hwnd,
			.uID = 1,
			.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,//TODO(fran): check this
			.uCallbackMessage = WM_TRAY,
			.hIcon = icon,
			.szTip = appnameL, //NOTE(fran): must be less than 128 chars
			.dwState = NIS_SHAREDICON,//TODO(fran): check this
			.dwStateMask = NIS_SHAREDICON,
			.szInfo = 0,
			.uVersion = NOTIFYICON_VERSION_4,//INFO(fran): this changes the message format, but not when nif_showtip is enabled, I think
			.szInfoTitle = 0,
			.dwInfoFlags = NIIF_NONE,
			//.guidItem = ,
			.hBalloonIcon = NULL,
		};

		b32 ret = Shell_NotifyIconW(NIM_ADD, &res.notification);
		assert(ret);

		//TODO(fran): Check this: when changing system dpi the tray icon gets stretched by Windows, which obviously being Windows means after some stretchings the icon is a complete blurry mess, can we update the icon on dpi change?

		return res;
	}

	internal void DestroyTrayIcon(tray_icon* tray) //TODO(fran): another name. RemoveTrayIcon?
	{
		BOOL ret = Shell_NotifyIconW(NIM_DELETE, &tray->notification);
		DestroyIcon(tray->notification.hIcon);
	}

	internal b32 UnregisterSystemGlobalHotkey(OS::window_handle wnd, i32 id)
	{
		b32 res = ::UnregisterHotKey(wnd.hwnd, id);
		return res;
	}

	internal b32 RegisterSystemGlobalHotkey(OS::window_handle wnd, OS::hotkey_data hotkey, i32 hotkey_id)
	{
		b32 res = RegisterHotKey(wnd.hwnd, hotkey_id, hotkey.mods | MOD_NOREPEAT, hotkey.vk); 
		//NOTE(fran): if you dont associate the hotkey with any hwnd then it just sends the msg to the current thread with hwnd nil, also documentation says it can not associate the hotkey with a wnd created from another thread
		return res;
	}

	internal s8 GetUIFontName() {
		//Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
		//-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
		//-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
		//-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

		HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); };

		struct search_font {
			const char** fontname_options;
			u32 fontname_cnt;
			u32 best_match;
			const char* match;
		};

		local_persistence const char* default_font = "";
		local_persistence const char* requested_fontnames[] = { "Segoe UI", "Arial Unicode MS", "Microsoft YaHei", "Microsoft YaHei UI", "Microsoft JhengHei", "Microsoft JhengHei UI" };

		search_font request = {
			.fontname_options = requested_fontnames,
			.fontname_cnt = ArrayCount(requested_fontnames),
			.best_match = U32MAX,
			.match = default_font,
		};

		//TODO(fran): this is stupid, there must be a way to question about a specific font without having to go through all of them. Also, can we do this with DirectWrite instead of Gdi?
		EnumFontFamiliesExA(dc, nil
			, [](const LOGFONTA* lpelfe, const TEXTMETRICA* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)->int
			{
				auto req = ((search_font*)lParam);
				for (u32 i = 0; i < req->fontname_cnt; i++)
				{
					//if (_wcsicmp(req->fontname_options[i], lpelfe->lfFaceName) == 0) {
					if (_stricmp(req->fontname_options[i], lpelfe->lfFaceName) == 0) {
						if (i < req->best_match)
						{
							req->best_match = i;
							req->match = req->fontname_options[i];
							if (i == 0) return false;
						}
					}
				}
				return true;
			}
		, (LPARAM)&request, 0);

		return const_temp_s((utf8*)request.match);
	}
}
