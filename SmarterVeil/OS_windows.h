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

#define WM_FUNC_HKTOSTR (WM_USER + 5)

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
	b32 operator!=(window_handle cmp_wnd) { return this->hwnd != cmp_wnd.hwnd; }
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

internal iu::ui_key VkToIuKey(u8 vk)
{
	switch (vk)
	{
#define _vk_k_mapping(mapping) \
	mapping(0, (iu::ui_key)0)					   \
	mapping(VK_ESCAPE, iu::ui_key::Esc)					   \
	mapping(VK_F1, iu::ui_key::F1)						   \
	mapping(VK_F2, iu::ui_key::F2)						   \
	mapping(VK_F3, iu::ui_key::F3)						   \
	mapping(VK_F4, iu::ui_key::F4)						   \
	mapping(VK_F5, iu::ui_key::F5)						   \
	mapping(VK_F6, iu::ui_key::F6)						   \
	mapping(VK_F7, iu::ui_key::F7)						   \
	mapping(VK_F8, iu::ui_key::F8)						   \
	mapping(VK_F9, iu::ui_key::F9)						   \
	mapping(VK_F10, iu::ui_key::F10)						   \
	mapping(VK_F11, iu::ui_key::F11)						   \
	mapping(VK_F12, iu::ui_key::F12)						   \
	mapping(VK_F13, iu::ui_key::F13)						   \
	mapping(VK_F14, iu::ui_key::F14)						   \
	mapping(VK_F15, iu::ui_key::F15)						   \
	mapping(VK_F16, iu::ui_key::F16)						   \
	mapping(VK_F17, iu::ui_key::F17)						   \
	mapping(VK_F18, iu::ui_key::F18)						   \
	mapping(VK_F19, iu::ui_key::F19)						   \
	mapping(VK_F20, iu::ui_key::F20)						   \
	mapping(VK_F21, iu::ui_key::F21)						   \
	mapping(VK_F22, iu::ui_key::F22)						   \
	mapping(VK_F23, iu::ui_key::F23)						   \
	mapping(VK_F24, iu::ui_key::F24)						   \
	mapping('A', iu::ui_key::A)							   \
	mapping('B', iu::ui_key::B)							   \
	mapping('C', iu::ui_key::C)							   \
	mapping('D', iu::ui_key::D)							   \
	mapping('E', iu::ui_key::E)							   \
	mapping('F', iu::ui_key::F)							   \
	mapping('G', iu::ui_key::G)							   \
	mapping('H', iu::ui_key::H)							   \
	mapping('I', iu::ui_key::I)							   \
	mapping('J', iu::ui_key::J)							   \
	mapping('K', iu::ui_key::K)							   \
	mapping('L', iu::ui_key::L)							   \
	mapping('M', iu::ui_key::M)							   \
	mapping('N', iu::ui_key::N)							   \
	mapping('O', iu::ui_key::O)							   \
	mapping('P', iu::ui_key::P)							   \
	mapping('Q', iu::ui_key::Q)							   \
	mapping('R', iu::ui_key::R)							   \
	mapping('S', iu::ui_key::S)							   \
	mapping('T', iu::ui_key::T)							   \
	mapping('U', iu::ui_key::U)							   \
	mapping('V', iu::ui_key::V)							   \
	mapping('W', iu::ui_key::W)							   \
	mapping('X', iu::ui_key::X)							   \
	mapping('Y', iu::ui_key::Y)							   \
	mapping('Z', iu::ui_key::Z)							   \
	mapping('0', iu::ui_key::_0)							   \
	mapping('1', iu::ui_key::_1)							   \
	mapping('2', iu::ui_key::_2)							   \
	mapping('3', iu::ui_key::_3)							   \
	mapping('4', iu::ui_key::_4)							   \
	mapping('5', iu::ui_key::_5)							   \
	mapping('6', iu::ui_key::_6)							   \
	mapping('7', iu::ui_key::_7)							   \
	mapping('8', iu::ui_key::_8)							   \
	mapping('9', iu::ui_key::_9)							   \
	mapping(VK_NUMPAD0, iu::ui_key::Numpad0)				   \
	mapping(VK_NUMPAD1, iu::ui_key::Numpad1)				   \
	mapping(VK_NUMPAD2, iu::ui_key::Numpad2)				   \
	mapping(VK_NUMPAD3, iu::ui_key::Numpad3)				   \
	mapping(VK_NUMPAD4, iu::ui_key::Numpad4)				   \
	mapping(VK_NUMPAD5, iu::ui_key::Numpad5)				   \
	mapping(VK_NUMPAD6, iu::ui_key::Numpad6)				   \
	mapping(VK_NUMPAD7, iu::ui_key::Numpad7)				   \
	mapping(VK_NUMPAD8, iu::ui_key::Numpad8)				   \
	mapping(VK_NUMPAD9, iu::ui_key::Numpad9)				   \
	mapping(VK_MULTIPLY, iu::ui_key::NumpadMultiply)		   \
	mapping(VK_ADD, iu::ui_key::NumpadAdd)				   \
	mapping(VK_SUBTRACT, iu::ui_key::NumpadSubtract)		   \
	mapping(VK_DECIMAL, iu::ui_key::NumpadDecimal)		   \
	mapping(VK_DIVIDE, iu::ui_key::NumpadDivide)			   \
	mapping(VK_CONTROL, iu::ui_key::Ctrl)				   \
	mapping(VK_SHIFT, iu::ui_key::Shift)					   \
	mapping(VK_MENU, iu::ui_key::Alt)					   \
	mapping(VK_LWIN, iu::ui_key::OS)						   \
	mapping(VK_TAB, iu::ui_key::Tab)						   \
	mapping(VK_CAPITAL, iu::ui_key::CapsLock)			   \
	mapping(VK_NUMLOCK, iu::ui_key::NumLock)				   \
	mapping(VK_SCROLL, iu::ui_key::ScrollLock)			   \
	mapping(VK_UP, iu::ui_key::UpArrow)					   \
	mapping(VK_DOWN, iu::ui_key::DownArrow)				   \
	mapping(VK_LEFT, iu::ui_key::LeftArrow)				   \
	mapping(VK_RIGHT, iu::ui_key::RightArrow)			   \
	mapping(VK_OEM_7, iu::ui_key::Apostrophe)			   \
	mapping(VK_OEM_COMMA, iu::ui_key::Comma)				   \
	mapping(VK_OEM_PERIOD, iu::ui_key::Period)			   \
	mapping(VK_OEM_1, iu::ui_key::Semicolon)				   \
	mapping(VK_OEM_MINUS, iu::ui_key::Minus)				   \
	mapping(VK_OEM_PLUS, iu::ui_key::Equal) /*TODO(fran): wtf, on US it is =, on Spanish it is + */		\
	mapping(VK_OEM_4, iu::ui_key::LeftBracket)			   \
	mapping(VK_OEM_6, iu::ui_key::RightBracket)			   \
	mapping(VK_OEM_5, iu::ui_key::Backslash)				   \
	mapping(VK_OEM_2, iu::ui_key::Forwardslash)			   \
	mapping(VK_OEM_3, iu::ui_key::GraveAccent)			   \
	mapping(VK_OEM_102, iu::ui_key::LessThan)			   \
	mapping(VK_PRIOR, iu::ui_key::PageUp)				   \
	mapping(VK_NEXT, iu::ui_key::PageDown)				   \
	mapping(VK_HOME, iu::ui_key::Home)					   \
	mapping(VK_END, iu::ui_key::End)					   \
	mapping(VK_INSERT, iu::ui_key::Insert)				   \
	mapping(VK_DELETE, iu::ui_key::DeleteForward)		   \
	mapping(VK_BACK, iu::ui_key::DeleteBackward)		   \
	mapping(VK_SPACE, iu::ui_key::Space)				   \
	mapping(VK_RETURN, iu::ui_key::Enter)				   \
	mapping(VK_SNAPSHOT, iu::ui_key::PrintScreen)		   \
	mapping(VK_PAUSE, iu::ui_key::Pause)		   \
	mapping(VK_APPS, iu::ui_key::ContextMenu)			   \
	mapping(VK_VOLUME_UP, iu::ui_key::VolumeUp)			   \
	mapping(VK_VOLUME_DOWN, iu::ui_key::VolumeDown)		   \
	mapping(VK_VOLUME_MUTE, iu::ui_key::VolumeMute)		   \
	mapping(VK_MEDIA_NEXT_TRACK, iu::ui_key::MediaNextTrack)\
	mapping(VK_MEDIA_PREV_TRACK, iu::ui_key::MediaPrevTrack)\
	mapping(VK_MEDIA_PLAY_PAUSE, iu::ui_key::MediaPlayPause)\
	mapping(VK_MEDIA_STOP, iu::ui_key::MediaStop)

	//	return iu::ui_key::NumpadEnter; //TODO(fran): this is VK_RETURN + the extended key flag
#define _vktok(vk, k) case vk : return k;
	_vk_k_mapping(_vktok);

	//NOTE(fran): we can do one to many mapping, but unfortunately not many to one because the compiler will throw an error saying that case statement already exists, so we gotta manually add the one to many mappings here (but not in IuKeyToVk)
	_vktok(VK_LCONTROL, iu::ui_key::Ctrl)
	_vktok(VK_RCONTROL, iu::ui_key::Ctrl)
	_vktok(VK_LSHIFT, iu::ui_key::Shift)
	_vktok(VK_RSHIFT, iu::ui_key::Shift)
	_vktok(VK_LMENU, iu::ui_key::Alt)
	_vktok(VK_RMENU, iu::ui_key::Alt)
	_vktok(VK_RWIN, iu::ui_key::OS)
	default:
	{
		OutputDebugStringA("Win32 WARNING: unhandled virtual key: ");
		OutputDebugStringA((char*)VkToString(vk).chars);
		OutputDebugStringA("\n");
		return (iu::ui_key)0; //TODO(fran): return null key? tell the caller not to use the key? leave it like this?
	}
	}
}

internal u8 IuKeyToVk(iu::ui_key k)
{
	switch (k)
	{
#define _ktovk(vk, k) case k : return vk;
		_vk_k_mapping(_ktovk);
	default:
	{
		OutputDebugStringA("[WARNING][Win32]: unhandled Iu key with value: ");
		char buf[16]; sprintf_s(buf, 16, "%d\n", k); OutputDebugStringA(buf); //TODO(fran): auto generated enum value names
		return 0; //TODO(fran): return null key? tell the caller not to use the key? leave it like this?
	}
	}
}

u16 IuKeyModsToVkMods(iu::ui_key_modifiers _mods) {
	u16 res = 
		(_mods & iu::ui_key_modifiers::Ctrl ? MOD_CONTROL : 0) 
		| (_mods & iu::ui_key_modifiers::Shift ? MOD_SHIFT : 0) 
		| (_mods & iu::ui_key_modifiers::Alt ? MOD_ALT : 0) 
		| (_mods & iu::ui_key_modifiers::OS ? MOD_WIN : 0);
	//NOTE(fran): hotkeys that include the Windows key are reserved for use by the OS, I add it just for completion but it will not be a valid hotkey
	//TODO(fran): faster conversion via bitshifting instead of conditionals
	return res;
}

struct hotkey_data {
	u8 vk;
	u16 mods;

	b32 is_valid_hotkey()
	{
		b32 res = this->vk || this->mods;
		return res;
	}

	b32 operator!=(hotkey_data hotkey_to_compare)
	{
		b32 res = this->vk != hotkey_to_compare.vk || this->mods != hotkey_to_compare.mods;
		return res;
	}
	hotkey_data(iu::ui_hotkey_data _hk)
	{
		this->vk = IuKeyToVk(_hk.key);
		this->mods = IuKeyModsToVkMods(_hk.mods);
	}
};

//TODO(fran): #define max_hotkey_char_cnt 128 (or smth like that)
internal void _HotkeyToString(_OS::hotkey_data hk, s8* hotkey_str)
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
	if (hk.mods & MOD_WIN) {
		if (!first_mod) *hotkey_str += u8"+";
		*hotkey_str += u8"Windows";
		first_mod = false;
	}

	u8 vk = hk.vk;

	s8 vk_str = stack_s(decltype(vk_str), 128);

	wchar_t vk_str16[128];
#if 0 //this is the correct way to do mapping, but since we arent sending the scancode to Iu we cant use it
	//TODO(fran): we maaay want to have this stored in the language file instead?
	int keynameres = GetKeyNameTextW((LONG)(hk.translation_nfo & (~(LPARAM)(1 << 25))), vk_str16, ArrayCount(vk_str16)); //NOTE(fran): set bit 25 to 0 to indicate we dont care about left or right keys (eg Left vs Right Ctrl)
	//vk_str16[keynameres++] = L'ー';
#endif

		//TODO(fran): this is close enough, only thing missing now is that the language doesnt seem to get updated, we have to close and reopen the app before Windows changes our language, we are probably missing some response to WM_INPUTLANGUAGECHANGED or smth like that that modifies some value in our process since both MapVirtualKeyExW and GetKeyNameTextW fail to change language
	int keynameres = 0;
	u32 scancode = ::MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC, ::GetKeyboardLayout(/*::GetThreadId(_OS::thread_setup.io_thread)*/0)); //INFO(fran): GetKeyboardLayout gets buggy if we pass it a thread id different from the current thread (in our case the io_thread)
	//assert(scancode == ((hk.translation_nfo >> 16) & 0xFF)); //check scancode vs scancode always match, what's expected to not match is the extended key flag

	switch (vk) //Reference: https://stackoverflow.com/questions/38100667/windows-virtual-key-codes
	{
	case VK_VOLUME_MUTE: case VK_VOLUME_DOWN: case VK_VOLUME_UP:
	case VK_MEDIA_NEXT_TRACK: case VK_MEDIA_PREV_TRACK: case VK_MEDIA_STOP: case VK_MEDIA_PLAY_PAUSE:
		//TODO(fran): there are probably more keys that cant be mapped, like the VK_BROWSER_... but Iu currently doesnt handle them
	{
		//NOTE(fran): some virtual keys are outright unmappable by Windows for some strange reason, even the scancode is simply 0 and has the extended bit set, wtf Windows!? For those ones we use our own (English language only) mapping
		//Another strange thing is that VK_PAUSE _does_ have a scancode mapping but still MapVirtualKeyExW cannot map it, only god knows the answer to that one, or maybe Raymond Chen. 
		//Found the reason: https://handmade.network/forums/articles/t/2823-keyboard_inputs_-_scancodes%252C_raw_input%252C_text_input%252C_key_names we should do MapVirtualKeyExW(0xe100 | hk.vk, MAPVK_VK_TO_VSC_EX, ...); there's only one problem MAPVK_VK_TO_VSC_EX only works on Vista & later, thanks Windows
		//const s8 stringed_vk = VkToString(vk);
		//vk_str += stringed_vk;
	} break;
	case VK_PAUSE: //TODO(fran): find out if this always works
		scancode = 0x45;
		goto default_case;
	case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
	case VK_RCONTROL: case VK_RMENU:
	case VK_LWIN: case VK_RWIN: case VK_APPS:
	case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
	case VK_INSERT: case VK_DELETE:
	case VK_DIVIDE: case VK_NUMLOCK:
		//NOTE(fran): other keys need the extended bit set, I have no clue why Windows' mapper doesnt set it
		scancode |= KF_EXTENDED;
	default:
	default_case:
		keynameres += GetKeyNameTextW(scancode << 16, vk_str16 + keynameres, ArrayCount(vk_str16) - keynameres);
	}

	if (keynameres) {
		//TODO(fran): 
		// if keynameres > 2 && IS_HIGH_SURROGATE(vk_str[0]) and advance +2 & keynameres-1
		//TODO(fran): uppercase the first character, there are cases where it's lowercase (ej ñ)
		if (keynameres > 1) _wcslwr_s(vk_str16 + 1, keynameres); //TODO(fran): better lowercase functions, and work with locales

		s8 vk_str8 = _OS::convert_to_s8({ .chars = (utf16*)vk_str16, .cnt = (u64)keynameres }); defer{ _OS::free_small_mem(vk_str8.chars); }; //TODO(fran): convert in place, straight to vk_str

		vk_str += vk_str8;
	}
	else {
		const s8 stringed_vk = VkToString(vk);
		vk_str += stringed_vk;
	}

	if (vk_str.cnt) {
		if (!first_mod) *hotkey_str += u8"+";
		*hotkey_str += vk_str;
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

	//TODO(fran): Edge case: when Alt+tabbing out we wont receive the Alt up msg, we should therefore manually push an Alt up key event on WM_KILLFOCUS
		//Reference: https://handmade.network/forums/articles/t/2823-keyboard_inputs_-_scancodes%252C_raw_input%252C_text_input%252C_key_names
	case WM_FUNC_HKTOSTR:
	{
		struct _hktostr { _OS::hotkey_data hk; s8* hotkey_str; };
		_hktostr* hktostr = (decltype(hktostr))wParam;
		_HotkeyToString(hktostr->hk, hktostr->hotkey_str);
	} break;
	default:
	{
		default_handling:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	} break;
	}
	return 0;
}

internal OS::window_handle _Window(OS::window_creation_flags flags)
{
	OS::window_handle res{ 0 };

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
		extended_flags |= (flags & OS::topmost) ? WS_EX_TOPMOST : 0;
#endif
		extended_flags |= (flags & OS::notaskbar) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
		extended_flags |= (flags & OS::clickthrough) ? WS_EX_LAYERED | WS_EX_TRANSPARENT : 0;

#if 1
		u32 basic_flags = WS_POPUP;
#else //os_managed
		u32 basic_flags = WS_OVERLAPPEDWINDOW;
#endif
		basic_flags |= (flags & OS::resizeborder) ? WS_THICKFRAME | WS_MAXIMIZEBOX : 0; //TODO(fran): separate WS_MAXIMIZEBOX as a different flag?

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

declaration internal void io_thread_handler();//TODO(fran): HACK: remove

struct io_thread_setup {
	HANDLE io_thread;
	HWND dummy_wnd; //handled by the io_thread //TODO(fran): create it in our constructor to avoid edge case race conditions on startup
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

definition internal void io_thread_handler()
{
	_OS::thread_setup.dummy_wnd = _Window(OS::notaskbar).hwnd; //TODO(fran): destroy the window
	if (!_OS::thread_setup.dummy_wnd) crash(); //TODO(fran): may only crash this thread, use TerminateProcess()
	::ShowWindow(_OS::thread_setup.dummy_wnd, SW_HIDE);

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
				SetCapture(msg.hwnd); //NOTE(fran): SetCapture must be called from the same thread that created the hwnd
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
			//TODO(fran) : may be good to send WM_NCMOUSEMOVE as mousemove to the ui too, to handle cases where the mouse goes from the client area to the nonclient
			//TODO(fran)?: send WM_NCMOUSEMOVE WM_NCLBUTTONDOWN messages to the ui as normal mouse messages and have the ui code decide when to send this real messages to the wndproc, that way we allow for the ui to decide where the OS features should be active, allowing for, for example, have the close/max/min buttons block the resize border in their region
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
			case WM_CHAR: //TODO(fran): WM_UNICHAR
			{
				//NOTE(fran): WM_CHAR is generated when WM_KEYDOWN messages are passed to TranslateMessage, the character is in the wParam and encoded in UTF-16. If the codepoint is 4 bytes, there are 2 WM_CHAR messages, one with the high surrogate and one with the low surrogate. The surrogate on its own is not valid unicode and cannot be converted to utf8, we have to wait till both the high and low surrogate are joined before doing the conversion
				
				s16 chars16 = stack_s(decltype(chars16), 2);

				auto PushTextEvents = [&]() {
					assert(chars16.cnt > 0 && chars16.cnt <= 2);
					utf8 c[4];
					i32 cnt = WideCharToMultiByte(CP_UTF8, 0, (WCHAR*)chars16.chars, chars16.cnt, (char*)c, ArrayCount(c), 0, 0) / sizeof(*c);
					assert(cnt);
					for (i32 i = 0; i < cnt; i++) iu::PushEventText(wnd, c[i]);
				};

				utf16 c16 = msg.wParam;

				local_persistence utf16 highsurrogate = 0;

				if (IS_HIGH_SURROGATE(c16))
				{
					highsurrogate = c16;
				}
				else if (IS_LOW_SURROGATE(c16))
				{
					if (IS_SURROGATE_PAIR(highsurrogate, c16))
					{
						chars16 += highsurrogate;
						chars16 += c16;
						PushTextEvents();
					}
					highsurrogate = 0;
				}
				else
				{
					chars16 += c16;
					PushTextEvents();
				}

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
				iu::PushEventKey(wnd, _OS::VkToIuKey(vk), iu::ui_key_state::clicked, msg.lParam);
			} break;
			case WM_SYSKEYUP:
			case WM_KEYUP:
			{
				u8 vk = msg.wParam;
				iu::PushEventKey(wnd, _OS::VkToIuKey(vk), iu::ui_key_state::unclicked, msg.lParam);
			} break;
			}
			DispatchMessageW(&msg);
			//TODO(fran): test the delay of the dispatch compared to just proccesing the msgs from here as we currently do, if it isnt too bad (<0.01ms) then join all the msg processing inside the wndproc
		}
		::MsgWaitForMultipleObjectsEx(0, nil, INFINITE, QS_ALLINPUT, 0 | MWMO_ALERTABLE | MWMO_INPUTAVAILABLE); //TODO(fran): MWMO_INPUTAVAILABLE
	}
}

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

	LARGE_INTEGER _QueryPerformanceFrequency()
	{
		LARGE_INTEGER res{};
		QueryPerformanceFrequency(&res);//NOTE(fran): always works on Windows XP and above
		return res;
	}
}

//NOTE(fran): services provided by the OS to the application

//IMPORTANT: for filepaths use / instead of \\, since / should be supported by default in all OSs

namespace OS
{
	internal s8 GetWorkFolder() //TODO(fran): not yet sure whether we want to provide this functionality or not
	{
		s8 res = _OS::work_folder.dir_str;
		return res;
	}
	//Returns value in milliseconds (1.0f == 1ms)
	internal f32 GetSleepResolution() //TODO(fran): not yet sure whether we want to provide this functionality or not
	{
		f32 res = _OS::set_sleep_resolution.desired_scheduler_ms;
		return res;
	}

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

		if (res) res = MoveFileExW((wchar_t*)temp.chars, (wchar_t*)windows_filename.chars, MOVEFILE_REPLACE_EXISTING); //TODO(fran): MOVEFILE_COPY_ALLOWED
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
				void* mem = VirtualAlloc(0, sz32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);//TODO(fran): READONLY?
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

	constexpr u32 locale_max_length = LOCALE_NAME_MAX_LENGTH * 2; /*utf8 length*/
	//NOTE: the string must be able to contain OS::locale_max_length characters
	internal void GetUserLocale(s8* res)
	{
		assert((res->cnt_allocd - res->cnt) >= locale_max_length);
		utf16 loc16[LOCALE_NAME_MAX_LENGTH]; loc16[0] = 0;
		GetUserDefaultLocaleName((wchar_t*)loc16, ArrayCount(loc16));

		s8 loc = _OS::convert_to_s8(temp_s(loc16)); defer{ _OS::free_small_mem(loc.chars); };
		*res += loc;
	}

	internal OS::window_handle Window(OS::window_creation_flags creation_flags)
	{
		OS::window_handle res;

		struct _create_wnd { OS::window_handle wnd; OS::window_creation_flags flags; b32 done; } volatile create_wnd{ .wnd = {}, .flags = creation_flags, .done = false };

		//TODO(fran): instead of doing this crazy stuff I could hack it, do a SendMessage to a hidden window we create on startup from the other thread & that lives through the entirety of the program

		::QueueUserAPC(
			[](ULONG_PTR args) {
				_create_wnd* create_wnd = (decltype(create_wnd))args;
				create_wnd->wnd = _OS::_Window(create_wnd->flags);
				create_wnd->done = true;
			}
		, _OS::thread_setup.io_thread, (ULONG_PTR)&create_wnd);

		while (!create_wnd.done) {}

		res = *(OS::window_handle*)(&create_wnd.wnd); //NOTE(fran): removing volatile before being able to copy to a non volatile, this is so dumb

		assert(res);

		return res;
	}

	internal void HotkeyToString(iu::ui_hotkey_data hk, s8* hotkey_str)
	{
		//TODO(fran): HACK: handle WM_INPUTLANGCHANGE from the io_thread and change the main thread's language, we cant be calling the io_thread each time we need a hotkey translated or our performance will fall to the floor
		struct _hktostr { _OS::hotkey_data hk; s8* hotkey_str; } volatile hktostr{ .hk = _OS::hotkey_data(hk), .hotkey_str = hotkey_str };

		SendMessageW(_OS::thread_setup.dummy_wnd, WM_FUNC_HKTOSTR, (WPARAM)&hktostr, 0);
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
	//TODO(fran): FullscreenWindow (cover the whole screen)
	//TODO(fran): Maximize to all monitors

	internal void RestoreWindow(window_handle wnd)
	{
		ShowWindow(wnd.hwnd, SW_RESTORE);
	}

	// Minimizes a window and creates an animation to make it look like it goes to the tray
	internal void MinimizeWindowToTray(window_handle wnd)
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
	internal void RestoreWindowFromTray(window_handle wnd)
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
		b32 res = ::IsWindowVisible(wnd.hwnd);
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

	//TODO(fran): HACK
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

	internal f32 GetScalingForWindow(OS::window_handle wnd)
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
			.szInfo = {},
			.uVersion = NOTIFYICON_VERSION_4,//INFO(fran): this changes the message format, but not when nif_showtip is enabled, I think
			.szInfoTitle = {},
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
		b32 res;
		struct _unreg_hk { OS::window_handle wnd; i32 id; b32 ret; } volatile unreg{ .wnd = wnd, .id = id, .ret = -10 };

		//NOTE(fran): both Register & Unregister global hotkey functions need to be called from the input thread because hotkey msgs are posted to the thread that called the functions
		::QueueUserAPC( //TODO(fran): in the end SendMessage might simply be a better option save for the caveat of having to define custom messages in the os code. Also QueueUserAPC has a big problem, if the other thread is stalled by the os (eg when moving a window) QueueUserAPC stalls also, we cannot continue to use it
			[](ULONG_PTR args) {
				_unreg_hk* unreg = (decltype(unreg))args;
				unreg->ret = ::UnregisterHotKey(unreg->wnd.hwnd, unreg->id);
			}
		, _OS::thread_setup.io_thread, (ULONG_PTR)&unreg);

		while (unreg.ret == -10) {}
		res = unreg.ret;
		return res;
	}

	internal b32 RegisterSystemGlobalHotkey(OS::window_handle wnd, iu::ui_hotkey_data hotkey, i32 hotkey_id)
	{
		b32 res;
		struct _reg_hk { OS::window_handle wnd; _OS::hotkey_data hotkey; i32 hotkey_id; b32 ret; } volatile reg{ .wnd = wnd, .hotkey = _OS::hotkey_data(hotkey), .hotkey_id = hotkey_id, .ret = -10 };

		::QueueUserAPC(
			[](ULONG_PTR args) {
				_reg_hk* reg = (decltype(reg))args;
				reg->ret = ::RegisterHotKey(reg->wnd.hwnd, reg->hotkey_id, reg->hotkey.mods | MOD_NOREPEAT, reg->hotkey.vk);
				//TODO(fran): we may want to leave the hwnd as nil and say that global hotkeys are gonna be iu global (inside iu_state) & outside the ui_state
					//NOTE(fran): if you dont associate the hotkey with any hwnd then it just sends the msg to the current thread with hwnd nil, also documentation says it can not associate the hotkey with a wnd created from another thread
			}
		, _OS::thread_setup.io_thread, (ULONG_PTR)&reg);

		while (reg.ret == -10) {}
		res = reg.ret;
		return res;
	}

	internal s8 GetUIFontName() {
		//Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
		//-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
		//-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
		//-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

		HDC dc = ::GetDC(::GetDesktopWindow()); defer{ ::ReleaseDC(::GetDesktopWindow(),dc); };

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

	struct nc_margins { f32 leftwidth, topheight, rightwidth, bottomheight; };
	//Identifies the standard dimensions of the non client area of a window, taking into account that window's dpi
	internal nc_margins GetNonClientMargins(f32 dpi_scaling)
	{
		nc_margins res;
		
		RECT testrc{ 0,0,100,100 };
		RECT ncrc = testrc;

		local_persistence f32 dpicorrection = OS::GetScalingForSystem(); assert(dpicorrection);
		//NOTE(fran): AdjustWindowRect retains the dpi of the system when the app started, therefore if we started on a 125% dpi system AdjustWindowRect will _always_ return the value scaled by 1.25f, and so on, in order to correct for that we retrieve the system dpi at the time of our app starting and divide by it to obtain the unscaled result
			//TODO(fran): BUG: this correction is close but not quite correct
				//Replicate: start with 100% dpi, change to 150%, h will be 46.5f
				//           start with 150% dpi, h will be 45.0f  

		//TODO(fran): AdjustWindowRectExForDpi
		AdjustWindowRect(&ncrc, WS_OVERLAPPEDWINDOW, FALSE/*no menu*/);

		res.leftwidth = (distance(testrc.left, ncrc.left) / dpicorrection) * dpi_scaling;
		res.topheight = (distance(testrc.top, ncrc.top) / dpicorrection) * dpi_scaling;
		res.rightwidth = (distance(testrc.right, ncrc.right) / dpicorrection) * dpi_scaling;
		res.bottomheight = (distance(testrc.bottom, ncrc.bottom) / dpicorrection) * dpi_scaling;

		return res;
	}

	//Returns the window the user is currently interacting with, which receives mouse & keyboard input (this window may or may not belong to the current process)
	internal window_handle GetForegroundWindow()
	{
		window_handle res = { ::GetForegroundWindow() };
		return res;
	}

	internal void SetForegroundWindow(window_handle wnd)
	{
		::SetForegroundWindow(wnd.hwnd);
		//TODO(fran): the new foreground window's thread gets a little more priority from Windows, unfortunately that thread will be our io_thread which we really dont care to prioritize, we'd prefer to prioritize the main thread instead if possible
	}

	enum class counter_timescale { seconds = 1, milliseconds = 1000, microseconds = 1000000, }; //NOTE(fran): all OSes must provide the same enum names, but the values might need to be different //TODO(fran): can we provide the enum value names (seconds, milliseconds, microseconds) in the general OS .h file and modify their values in the OS specific code (seconds = 1, milliseconds = 1000, microseconds = 1000000)?
	internal f64 GetPCFrequency(counter_timescale timescale)
	{
		local_persistence LARGE_INTEGER li = _OS::_QueryPerformanceFrequency(); //NOTE(fran): fixed value, can be cached
		return f64(li.QuadPart) / (f64)timescale;
	}
	internal i64 StartCounter()
	{
		LARGE_INTEGER li;
		::QueryPerformanceCounter(&li);
		return li.QuadPart;
	}
	internal f64 EndCounter(i64 CounterStart, counter_timescale timescale = counter_timescale::milliseconds)
	{
		LARGE_INTEGER li;
		::QueryPerformanceCounter(&li);
		return double(li.QuadPart - CounterStart) / GetPCFrequency(timescale);
	}
}
