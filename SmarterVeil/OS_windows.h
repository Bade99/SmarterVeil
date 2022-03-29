#pragma once

#include "win32_sdk.h"
#include "resource.h"
#include <shlobj_core.h> // SHGetKnownFolderPath
#include <shellapi.h> // NOTIFYICONDATA (everything related to the tray icon)

#define WM_GETTOPMARGIN (WM_USER+1)

#define WM_DPICHANGED_CUSTOM (WM_USER+51) /*lparam=is set to zero*/ /*NOTE(fran): PostMessage does _not_ allow to send WM_DPICHANGED because the lparam is a pointer that will only be valid while we remain on the wndproc scope*/

#define WM_TRAY (WM_USER + 101)

#define WM_CUSTOMSETCURSOR (WM_USER + 201) /*wParam=HCURSOR*/

//NOTE(fran): Windows API calls that need chaging for Dpi aware versions:
	// Reference: https://docs.microsoft.com/en-us/windows/win32/hidpi/high-dpi-desktop-application-development-on-windows
	// Single DPI version	Per - Monitor version
	// GetSystemMetrics	    GetSystemMetricsForDpi
	// AdjustWindowRectEx	AdjustWindowRectExForDpi
	// SystemParametersInfo	SystemParametersInfoForDpi
	// GetDpiForMonitor	    GetDpiForWindow


//NOTE(fran): helpers for the OS itself, should not be used by outside callers
namespace _OS
{
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

	LRESULT CALLBACK UIProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
#define MovWindow(wnd, rc) MoveWindow(wnd, rc.left, rc.top, RECTW(rc), RECTH(rc), false)
		//auto _msgName = msgToString(message); OutputDebugStringA(_msgName); OutputDebugStringA("\n");

		//TODO(fran): do not allow the window to get smaller than a certain threshold, currently the user can make the window be almost 0 width & 0 height

		//TODO(fran): add functionality that Windows doesnt provide: double click on _any_ of the resizing borders causes a window resize in that direction up to the corresponding screen bound, eg. double click that generates HTLEFT moves the window rect's left component to the left of the monitor it's in

		local_persistence i32 top_margin = 6; //TODO(fran): unique top_margin for each hwnd
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
				top_margin = RECTH(emptyrc) / 2;

				RECT winrc; GetWindowRect(hWnd, &winrc);
				RECT clirc; GetClientRect(hWnd, &clirc);

				calcsz->rgrc[0].top -= top_margin;
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
				RECT top_border = { 0, 0, RECTW(rc), top_margin };
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
		case WM_DESTROY:
		{
			PostQuitMessage(0); //TODO(fran): not all ui windows should be able to completely close the application
		} break;
		case WM_GETTOPMARGIN:
		{
			return top_margin;
		} break;
		case WM_TRAY:
		{
			//NOTE(fran): Windows strikes again! the tray message is sent straight to the proc bypassing the message queue
			PostMessageW(hWnd, message, wParam, lParam);
		} break;
		case WM_DPICHANGED:
		{
			//NOTE(fran): Uhh Windows, Windows... why do you do this to yourself?
			PostMessageW(hWnd, WM_DPICHANGED_CUSTOM, wParam, 0);

			//TODO(fran): DefWindowProcW does not seem to do the window resizing for us, wtf? I have to do it manually
			RECT suggested_window = *(RECT*)lParam;
			MovWindow(hWnd, suggested_window); //TODO(fran): test the rcs we get, I've seen that for many applications this suggested new window is terrible, both in position & size, see if we can come up with a better suggestion
		} break;
		case WM_CUSTOMSETCURSOR: { SetCursor((HCURSOR)wParam); } break;
		default:
		{
			return DefWindowProcW(hWnd, message, wParam, lParam);
		} break;
		}
		return 0;
#undef MovWindow
	}
}

//NOTE(fran): services provided by the OS to the application

//IMPORTANT: for filepaths use / instead of \\, since / should be supported by default in all OSs

namespace OS
{
	struct hotkey_data {
		u8 vk;    //Virtual Key
		u16 mods; //Hotkey modifiers: ctrl, alt, shift
		LPARAM translation_nfo; //Extra information needed by Windows when converting hotkey to text

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

		_work_folder() {
			dir_str = _OS::get_work_folder();

			s16 work_dir16 = _OS::convert_to_s16(dir_str); defer{ _OS::free_small_mem(work_dir16.chars); };
			_OS::windows_create_subdirectories(work_dir16);
			b32 res = _OS::windows_set_working_directory(work_dir16);
			assert(res);
		}

		~_work_folder() { _OS::free_small_mem(dir_str.chars); };
	}global_persistence work_folder;


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

		utf8 _vk_str[128];
		s8 vk_str{ .chars = _vk_str, .cnt = 0, .cnt_allocd = ArrayCount(_vk_str) };

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

	struct window_handle {
		HWND hwnd;
	};

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
			u32 extended_flags = WS_EX_NOREDIRECTIONBITMAP;
#ifndef DEBUG_BUILD
			extended_flags |= (flags & topmost) ? WS_EX_TOPMOST : 0;
#endif
			extended_flags |= (flags & notaskbar) ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
			extended_flags |= (flags & clickthrough) ? WS_EX_LAYERED | WS_EX_TRANSPARENT : 0;

			u32 basic_flags = WS_POPUP;
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
		res =
		{
			.x = (f32)clientrc.left,
			.y = (f32)clientrc.top,
			.w = (f32)RECTW(clientrc),
			.h = (f32)RECTH(clientrc),
		};
		return res;
	}

	internal rc2 GetWindowScreenRc(window_handle wnd) //TODO(fran): maybe also define one other function to return the screen client rect (non client area _not_ included)?
	{
		rc2 res;
		RECT clientrc; GetWindowRect(wnd.hwnd, &clientrc);
		res =
		{
			.x = (f32)clientrc.left,
			.y = (f32)clientrc.top,
			.w = (f32)RECTW(clientrc),
			.h = (f32)RECTH(clientrc),
		};
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
}
