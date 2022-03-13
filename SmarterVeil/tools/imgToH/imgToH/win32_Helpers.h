#pragma once

#include "win32_SDK.h"
#include "win32_Platform.h"

//---------------------ASSERTION----------------------:

#ifdef _DEBUG
#define UNCAP_ASSERTIONS
#endif

#ifdef UNCAP_ASSERTIONS
#define Assert(assertion) if(!(assertion))*(int*)NULL=0
#else 
#define Assert(assertion) 
#endif

//TODO(fran): for runtime asserts we could give the option to try to continue running anyway, if that were the case we must change the && for || and check against the return value of MessageBox

//"runtime" here means we guarantee this assertion will be executed in any build configuration with assertions enabled or not, since it's needed during runtime for the end user too
#define runtime_assert(assertion,msg) if (!(assertion))MessageBoxW(0,msg,L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) && (*(int*)NULL = 0)==0

//TODO(fran): this specific asserts are pretty stupid, when I understand sqlite better I should get rid of them
//NOTE: Neither the caller nor us should bother freeing anything since the program is going to stop execution
#define sqlite_exec_runtime_assert(errmsg) if (errmsg)MessageBoxW(0,(str(L"SQLite: ") + (utf16*)convert_utf8_to_utf16(errmsg,(int)(strlen(errmsg)+1)).str).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) && (*(int*)NULL = 0)==0

//Notifies the user on non terminal errors, as an extra frees the memory for the errmsg, TODO(fran): I dont know whether freeing is good or not
#define sqlite_exec_runtime_check(errmsg) if (errmsg){any_str err = convert_utf8_to_utf16(errmsg,(int)(strlen(errmsg)+1));MessageBoxW(0,(str(L"SQLite: ") + (utf16*)err.str).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND);free_any_str(err.str);sqlite3_free(errmsg);errmsg=0;}

#define sqliteok_runtime_assert(result_code,db) if ((result_code)!=SQLITE_OK)MessageBoxW(0,(str(L"SQLite: ") + (cstr*)sqlite3_errmsg16(db)).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) && (*(int*)NULL = 0)==0

#define sqliteok_runtime_check(result_code,db) if ((result_code)!=SQLITE_OK)MessageBoxW(0,(str(L"SQLite: ") + (cstr*)sqlite3_errmsg16(db)).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND)

#define sqlite_runtime_assert(assertion,db) if (!(assertion))MessageBoxW(0,(str(L"SQLite: ") + (cstr*)sqlite3_errmsg16(db)).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND) && (*(int*)NULL = 0)==0

#define sqlite_runtime_check(assertion,db) if (!(assertion))MessageBoxW(0,(str(L"SQLite: ") + (cstr*)sqlite3_errmsg16(db)).c_str(),L"Error",MB_OK|MB_ICONWARNING|MB_SETFOREGROUND)


//-------------Data retrieval from UI-----------------: (UI strings are always utf16)

// use for controls whose value is obtained via WM_GETTEXT, eg static, button, edit, ...
#define _get_edit_str(edit,any_str) \
			{ \
				int _sz_char = (int)SendMessageW(edit, WM_GETTEXTLENGTH, 0, 0) + 1; \
				any_str = alloc_any_str(_sz_char * sizeof(utf16)); \
				SendMessageW(edit, WM_GETTEXT, _sz_char, (WPARAM)any_str.str); \
			}

//----------------------STRING-----------------------:


#define to_str(v) std::to_wstring(v)

//NOTE: does _not_ include the null terminator
static size_t cstr_len(const cstr* s) {
	return wcslen(s);//TODO(fran): what about when cstr is char?
}

#define cstr_printf(...)  wprintf(__VA_ARGS__)

static bool str_found(size_t p) {
	return p != str::npos;
}

//NOTE: offset should be 1 after the last character of "begin"
//NOTE: returns str::npos if not found
static size_t find_closing_str(str text, size_t offset, str begin, str close) {
	//Finds closing str, eg: open= { ; close= } ; you're here-> {......{..}....} <-finds this one
	size_t closePos = offset - 1;
	i32 counter = 1;
	while (counter > 0 && text.size() > ++closePos) {
		if (!text.compare(closePos, begin.size(), begin)) {
			counter++;
		}
		else if (!text.compare(closePos, close.size(), close)) {
			counter--;
		}
	}
	return !counter ? closePos : str::npos;
}

//Returns first non matching char, could be 1 past the size of the string
static size_t find_till_no_match(str s, size_t offset, str match) {
	size_t p = offset - 1;
	while (s.size() > ++p) {
		size_t r = (str(1, s[p])).find_first_of(match.c_str());//NOTE: find_first_of is really badly designed
		if (!str_found(r)) {
			break;
		}
	}
	return p;
}

//True if "c" is in "chars", stops on the null terminator; if c == '\0' it returns false
static bool contains_char(cstr c, cstr* chars) {
	bool res = false;
	if (chars && *chars) { //TODO(fran): more compact
		do {
			if (c == *chars) {
				res = true;
				break;
			}
		} while (*++chars);
	}
	return res;
}

utf16_str to_utf16_str(utf16* s) { return { s, (cstr_len(s) + 1) * sizeof(*s) }; }
utf8_str to_utf8_str(utf8* s) { return { s, (strlen(s) + 1) * sizeof(*s) }; }

utf16_str to_utf16_str(std::wstring& s) { return { const_cast<decltype(&s[0])>(s.c_str()),(s.length() + 1) * sizeof(s[0]) }; }
utf8_str to_utf8_str(std::string& s) { return { const_cast<decltype(&s[0])>(s.c_str()),(s.length() + 1) * sizeof(s[0]) }; }

//TODO(fran): normalize all this convert functions into one that receives the necesssary params to achieve any conversion

//NOTE: the converted string needs to be freed via free_any_str

//INFO: For the string to be null-terminated sz must include the null terminator
static any_str convert_ascii_to_utf16(const char* s, int sz /*bytes*/) {
	using type = utf16;
	any_str res{0};
	int sz_char = MultiByteToWideChar(CP_ACP, 0, (LPCCH)s, sz, 0, 0);
	if (sz_char) {
		//TODO(fran): using parametric sizeof and decltype may not be the best idea, now we depend on the struct to not change say to void* if we wanted to have it be generic
		size_t sz_bytes = sz_char * sizeof(type);
		any_str temp_res = alloc_any_str(sz_bytes);
		if (temp_res.str) {
			MultiByteToWideChar(CP_ACP, 0, (LPCCH)s, sz, (LPWSTR)temp_res.str, sz_char);
			res = temp_res;
		}
	}
	return res;
}

//INFO: For the string to be null-terminated sz must include the null terminator
static any_str convert_utf8_to_utf16(const utf8* s, int sz /*bytes*/) {
	using type = utf16;
	any_str res{ 0 };
	int sz_char = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, sz, 0, 0);//NOTE: pre windows vista this may need a change
	if (sz_char) {
		size_t sz_bytes = sz_char * sizeof(type);
		any_str temp_res = alloc_any_str(sz_bytes);
		if (temp_res.str) {
			MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s, sz, (LPWSTR)temp_res.str, sz_char);
			res = temp_res;
		}
	}
	return res;
}

//INFO: For the string to be null-terminated sz must include the null terminator
//- sz in bytes
static any_str convert_utf16_to_utf8(const utf16* s, int sz /*bytes*/) {
	using type = utf8;
	any_str res{0};
	int sz_char = WideCharToMultiByte(CP_UTF8, 0, s, sz / sizeof(*s), 0, 0, 0, 0);//NOTE: pre windows vista this may need a change
	if (sz_char) {
		size_t sz_bytes = sz_char * sizeof(type);
		any_str temp_res = alloc_any_str(sz_bytes);
		if (temp_res.str) {
			WideCharToMultiByte(CP_UTF8, 0, s, sz / sizeof(*s), (LPSTR)temp_res.str, sz_char, 0, 0);
			res = temp_res;
		}
	}
	return res;
}
static s8 s16_to_s8(const s16& s) {
	return convert_utf16_to_utf8(s.str, (int)s.sz);
}

//----------------------DEFER-----------------------: (essential feature missing from the base language)

//Thanks to https://handmade.network/forums/t/1273-post_your_c_c++_macro_tricks/3
template <typename F> struct Defer { Defer(F f) : f(f) {} ~Defer() { f(); } F f; };
template <typename F> Defer<F> makeDefer(F f) { return Defer<F>(f); };
#define __defer( line ) defer_ ## line
#define _defer( line ) __defer( line )
struct defer_dummy { };
template<typename F> Defer<F> operator+(defer_dummy, F&& f) { return makeDefer<F>(std::forward<F>(f)); }
//usage: defer{block of code;}; //the last defer in a scope gets executed first (LIFO)
#define defer auto _defer( __LINE__ ) = defer_dummy( ) + [ & ]( )

//----------------------TRUNCATION-----------------------:

static u32 safe_u64_to_u32(u64 n) {
	Assert(n <= 0xFFFFFFFF); //TODO(fran): u32_max and _min
	return (u32)n;
}

//----------------------FILE-----------------------:

static void free_file_memory(void* memory) {
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}
struct read_entire_file_res { void* mem; u32 sz;/*bytes*/ };
//NOTE: free the memory with free_file_memory()
static read_entire_file_res read_entire_file(const cstr* filename) {
	read_entire_file_res res{ 0 };
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz)) {
			u32 sz32 = safe_u64_to_u32(sz.QuadPart);
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
static bool write_entire_file(const cstr* filename, void* memory, u32 mem_sz) {
	bool res = false;
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0)) {
			//SUCCESS
			res = mem_sz == bytes_written;
		}
		else {
			//TODO(fran): log
		}
	}
	return res;
}
static bool append_to_file(const cstr* filename, void* memory, u32 mem_sz) {
	bool res = false;
	HANDLE hFile = CreateFile(filename, FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
	if (hFile != INVALID_HANDLE_VALUE) {
		defer{ CloseHandle(hFile); };
		if (DWORD bytes_written; WriteFile(hFile, memory, mem_sz, &bytes_written, 0)) {
			//SUCCESS
			res = mem_sz == bytes_written;
		}
		else {
			//TODO(fran): log
		}
	}
	return res;
}

//----------------------TIMING-----------------------: (for testing/profiling)

static f64 GetPCFrequency() {
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);
	return f64(li.QuadPart) / 1000.0; //milliseconds
}
static i64 StartCounter() {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}
static f64 EndCounter(i64 CounterStart, f64 PCFreq = GetPCFrequency())
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return double(li.QuadPart - CounterStart) / PCFreq;
}


//----------------------RECT-----------------------:

#define RECTWIDTH(r) (r.right >= r.left ? r.right - r.left : r.left - r.right )

#define RECTHEIGHT(r) (r.bottom >= r.top ? r.bottom - r.top : r.top - r.bottom )

#define RECTW(r) RECTWIDTH(r)

#define RECTH(r) RECTHEIGHT(r)

static RECT rectWH(LONG left, LONG top, LONG width, LONG height) {
	RECT r;
	r.left = left;
	r.top = top;
	r.right = r.left + width;
	r.bottom = r.top + height;
	return r;
}

static RECT rectNpxL(RECT r, int n) {
	RECT res{ r.left,r.top,r.left + n,r.bottom };
	return res;
}
static RECT rectNpxT(RECT r, int n) {
	RECT res{ r.left,r.top,r.right,r.top + n };
	return res;
}
static RECT rectNpxR(RECT r, int n) {
	RECT res{ r.right - n,r.top,r.right,r.bottom };
	return res;
}
static RECT rectNpxB(RECT r, int n) {
	RECT res{ r.left ,r.bottom - n,r.right,r.bottom };
	return res;
}

static RECT rect1pxL(RECT r) {
	RECT res = rectNpxL(r, 1);
	return res;
}
static RECT rect1pxT(RECT r) {
	RECT res = rectNpxT(r, 1);
	return res;
}
static RECT rect1pxR(RECT r) {
	RECT res = rectNpxR(r, 1);
	return res;
}
static RECT rect1pxB(RECT r) {
	RECT res = rectNpxB(r, 1);
	return res;
}

#define BORDERLEFT		(1<<1)
#define BORDERTOP		(1<<2)
#define BORDERRIGHT		(1<<3)
#define BORDERBOTTOM	(1<<4)
#define BORDERALL		(BORDERLEFT|BORDERTOP|BORDERRIGHT|BORDERBOTTOM)
//NOTE: borders dont get centered, if you choose 5 it'll go 5 into the rc. TODO(fran): centered borders
static void FillRectBorder(HDC dc, RECT r, int thickness, HBRUSH br, int borders) {
	//TODO(fran): why the hell did I put thickness before br, FillRect has the br first!
	RECT borderrc;
	if (borders & BORDERLEFT) { borderrc = rectNpxL(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERTOP) { borderrc = rectNpxT(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERRIGHT) { borderrc = rectNpxR(r, thickness); FillRect(dc, &borderrc, br); }
	if (borders & BORDERBOTTOM) { borderrc = rectNpxB(r, thickness); FillRect(dc, &borderrc, br); }
}

static bool test_pt_rc(POINT p, RECT r) {
	bool res = false;
	if (p.y >= r.top &&//top
		p.y < r.bottom &&//bottom
		p.x >= r.left &&//left
		p.x < r.right)//right
	{
		res = true;
	}
	return res;
}



static bool rcs_overlap(RECT r1, RECT r2) {
	bool res = (r1.left < r2.right&& r1.right > r2.left && r1.top < r2.bottom&& r1.bottom > r2.top);
	return res;
}

static bool sameRc(RECT r1, RECT r2) {
	bool res = r1.bottom == r2.bottom && r1.left == r2.left && r1.right == r2.right && r1.top == r2.top;
	return res;
}

static RECT max_area(RECT* rcs, int len) { //Finds the rectangle with the biggest area in an array of RECTs, if two rcs have the same area it chooses the one first on the array //NOTE: if len is 0 it will crash
	int idx = 0;
	int max_area = 0;
	if (rcs) {
		for (int i = 0; i < len; i++) {
			RECT* test = rcs + i;
			int area = RECTWIDTH((*test)) * RECTHEIGHT((*test));
			if (area > max_area) { max_area = area; idx = i; }
		}
	}
	return *(rcs + idx); //TODO(fran): might be better to simply return the pointer
}

//Clips your rect "r" so it doesnt overlap the already present child windows of that parent
//"child" should be your wnd so we can filter it out
//NOTE: since we are talking about childs, the coordinates of "r" should be relative to the client area of the parent
static RECT clip_fit_childs(HWND parent, HWND child, RECT r) {
	RECT res;
	struct clip_fit_childs_test { HWND parent; HWND child; HRGN rgn; };
	clip_fit_childs_test t; t.parent = parent;  t.child = child; t.rgn = CreateRectRgn(r.left, r.top, r.right, r.bottom); defer{ DeleteObject(t.rgn); };

	EnumChildWindows(
		parent,
		[](HWND other_child, LPARAM lparam) {
			clip_fit_childs_test* test = (clip_fit_childs_test*)lparam;
			if (other_child != test->child && GetParent(other_child) == test->parent /*avoid enumerating childs of childs*/) {//TODO(fran): we should also filter non visible wnds I think, if everybody starts using this it could work well, maybe not
				RECT subtract_rc;
				GetClientRect(other_child, &subtract_rc);
				MapWindowPoints(other_child, test->parent, (LPPOINT)&subtract_rc, 2);

				HRGN subtract_rgn = CreateRectRgn(subtract_rc.left, subtract_rc.top, subtract_rc.right, subtract_rc.bottom); defer{ DeleteObject(subtract_rgn); };
				CombineRgn(test->rgn, test->rgn, subtract_rgn, RGN_DIFF);
			}
			return TRUE;
		},
		(LPARAM)&t);

	int rgn_len = GetRegionData(t.rgn, 0, 0);
	//INFO:RGNDATA always stores it's region polygon in RECTs
	RGNDATA* rgn_data = (RGNDATA*)malloc(rgn_len); defer{ free(rgn_data); };
	rgn_data->rdh.dwSize = sizeof(rgn_data->rdh); //TODO(fran): not sure I need to set anything in the header
	GetRegionData(t.rgn, rgn_len, rgn_data);
	//TODO(fran): check what happens when the region is empty, is there only one RECT with all zeroes or is it a null pointer and we crash
	if (rgn_data->rdh.nCount) res = max_area((RECT*)rgn_data->Buffer, rgn_data->rdh.nCount);
	else res = { 0,0,0,0 };

	return res;
}


//----------------------RECT_i32-----------------------:
#include "Vector.h"
union rect_i32 {
	struct { i32 x, y, w, h; };
	struct { i32 left, top, w, h; };
	struct { v2_i32 xy, wh; };

	i32 right() const { return left + w; }
	i32 bottom() const { return top + h; }
	i32 center_x() const { return left + w / 2; }
	i32 center_y() const { return top + h / 2; }
};

RECT to_RECT(rect_i32 r) {
	RECT res;
	res.left = r.left;
	res.top = r.top;
	res.right = r.right();
	res.bottom = r.bottom();
	return res;
}

rect_i32 to_rect_i32(RECT r) {
	rect_i32 res;
	res.left = r.left;
	res.top = r.top;
	res.w = RECTW(r);
	res.h = RECTH(r);

	return res;
}

static bool test_pt_rc(POINT p, const rect_i32& r) {
	bool res = false;
	if (p.y >= r.top &&//top
		p.y < r.bottom() &&//bottom
		p.x >= r.left &&//left
		p.x < r.right())//right
	{
		res = true;
	}
	return res;
}

//----------------------POINT-----------------------:
bool operator==(POINT p1, POINT p2) {
	bool res = p1.x == p2.x && p1.y == p2.y;
	return res;
}

bool operator!=(POINT p1, POINT p2) {
	bool res = !(p1 == p2);
	return res;
}

//----------------------SIZE-----------------------:
bool operator==(SIZE s1, SIZE s2) {
	bool res = s1.cx == s2.cx && s1.cy == s2.cy;
	return res;
}

bool operator!=(SIZE s1, SIZE s2) {
	bool res = !(s1 == s2);
	return res;
}

//----------------------HBRUSH-----------------------:

static COLORREF ColorFromBrush(HBRUSH br) {
	LOGBRUSH lb;
	GetObject(br, sizeof(lb), &lb);
	return lb.lbColor;
}

//----------------------ICON-----------------------:

struct MYICON_INFO //This is so stupid I find it hard to believe
{
	int     nWidth;
	int     nHeight;
	int     nBitsPerPixel;
};

static MYICON_INFO MyGetIconInfo(HICON hIcon)
{
	MYICON_INFO myinfo; ZeroMemory(&myinfo, sizeof(myinfo));

	ICONINFO info; ZeroMemory(&info, sizeof(info));

	BOOL bRes = FALSE; bRes = GetIconInfo(hIcon, &info);
	if (!bRes) return myinfo;

	BITMAP bmp; ZeroMemory(&bmp, sizeof(bmp));

	if (info.hbmColor)
	{
		const int nWrittenBytes = GetObject(info.hbmColor, sizeof(bmp), &bmp);
		if (nWrittenBytes > 0)
		{
			myinfo.nWidth = bmp.bmWidth;
			myinfo.nHeight = bmp.bmHeight;
			myinfo.nBitsPerPixel = bmp.bmBitsPixel;
		}
	}
	else if (info.hbmMask)
	{
		// Icon has no color plane, image data stored in mask
		const int nWrittenBytes = GetObject(info.hbmMask, sizeof(bmp), &bmp);
		if (nWrittenBytes > 0)
		{
			myinfo.nWidth = bmp.bmWidth;
			myinfo.nHeight = bmp.bmHeight / 2;
			myinfo.nBitsPerPixel = 1;
		}
	}

	if (info.hbmColor) DeleteObject(info.hbmColor);
	if (info.hbmMask) DeleteObject(info.hbmMask);

	return myinfo;
}

//----------------------HMENU-----------------------:

static BOOL SetMenuItemData(HMENU hmenu, UINT item, BOOL fByPositon, ULONG_PTR data) {
	MENUITEMINFOW i;
	i.cbSize = sizeof(i);
	i.fMask = MIIM_DATA;
	i.dwItemData = data;
	return SetMenuItemInfoW(hmenu, item, fByPositon, &i);
}

static BOOL SetMenuItemString(HMENU hmenu, UINT item, BOOL fByPositon, const cstr* str) {
	MENUITEMINFOW menu_setter;
	menu_setter.cbSize = sizeof(menu_setter);
	menu_setter.fMask = MIIM_STRING;
	menu_setter.dwTypeData = const_cast<cstr*>(str);
	BOOL res = SetMenuItemInfoW(hmenu, item, fByPositon, &menu_setter);
	return res;
}

//----------------------HWND-----------------------:

#define MyMoveWindow(wnd,xywh,repaint) MoveWindow(wnd, xywh.x, xywh.y, xywh.w, xywh.h, repaint)

#define MyMoveWindow_offset(wnd,xywh,repaint) MoveWindow(wnd, xywh.x, xywh.y + y_offset, xywh.w, xywh.h, repaint)

static void update_wnd_style(HWND wnd, LONG_PTR new_styles ) {
	SetWindowLongPtr(wnd, GWL_STYLE, GetWindowLongPtr(wnd, GWL_STYLE) | new_styles);
}

static void flip_visibility(HWND wnd) {
	bool visible = IsWindowVisible(wnd);
	ShowWindow(wnd, visible ? SW_HIDE : SW_SHOW);
}

static void SetText_txt_app(HWND wnd, const cstr* new_txt, const cstr* new_appname, bool txt_first = true) {
	if (new_txt == NULL || *new_txt == NULL) {
		SetWindowTextW(wnd, new_appname);
	}
	if (new_appname == NULL || *new_appname == NULL) {
		SetWindowTextW(wnd, new_txt);
	}
	else {
		if (!txt_first) {
			auto tmp = new_txt;
			new_txt = new_appname;
			new_appname = tmp;
		}
		str title_window = new_txt;
		title_window += L" - ";
		title_window += new_appname;
		SetWindowTextW(wnd, title_window.c_str());
	}
}

namespace MBP { //INFO: a way to avoid having redefinition errors for unscoped enum
	enum MBP {//MessageBox Placement
		left = (1 << 1),
		top = (1 << 2),
		right = (1 << 3),
		bottom = (1 << 4),
		center = (1 << 5),
	};
}
struct MsgBoxPlacement { HWND relativeTo; int MBP_flags; };
//INFO: MBP_flags defaults: left (if left or right or center isnt selected) & top (if top or bottom or center isnt selected)
//INFO: in relation to the previous, you can always expect this function to return a valid setter for x and y
static MsgBoxPlacement __msgbox_store_placement(HWND relativeTo = (HWND)INT32_MIN, int MBP_flags = INT32_MIN) { //TODO(fran): look for more elegant ways to send data from MessageBox to Hook_MsgBox
	static MsgBoxPlacement MBP{ 0 };
	if ((relativeTo != (HWND)INT32_MIN) && (MBP_flags != INT32_MIN)) {
		
		//Set defaults in case the user didnt do it
		//Flags for moving along x
		if (!(MBP_flags & MBP::left || MBP_flags & MBP::right || MBP_flags & MBP::center))//If one of this is not set
			MBP_flags |= MBP::left;//default to left
		//Flags for moving along y
		if (!(MBP_flags & MBP::top || MBP_flags & MBP::bottom || MBP_flags & MBP::center))//If one of this is not set
			MBP_flags |= MBP::top;//default to top

		MBP.relativeTo = relativeTo;
		MBP.MBP_flags = MBP_flags;
	}
	return MBP;
}
#include <CommCtrl.h> //SetWindowSubclass DefSubclassProc
//#include "windows_msg_mapper.h"
static LRESULT CALLBACK Hook_MsgBox(int code, WPARAM wparam, LPARAM lparam)
{
	//Thanks https://stackoverflow.com/questions/1530561/set-location-of-messagebox

	if (code == HCBT_CREATEWND)
	{
		CREATESTRUCT* pcs = ((CBT_CREATEWND*)lparam)->lpcs;

		if ((pcs->style & WS_DLGFRAME) || (pcs->style & WS_POPUP))//TODO(fran): this checks dont seem too robust
		{
			HWND wnd = (HWND)wparam;//Msgbox

			MsgBoxPlacement placement = __msgbox_store_placement();
			RECT rw; GetWindowRect(placement.relativeTo, &rw);
			int rel_x=rw.left;
			int rel_y=rw.top;
			int rel_w = RECTWIDTH(rw);
			int rel_h = RECTHEIGHT(rw);
			POINT p{0};//msgbox's new x and y

			if (placement.MBP_flags & MBP::center) {
				p.x = rel_x + rel_w / 2 - pcs->cx / 2;
				p.y = rel_y + rel_h / 2 - pcs->cy / 2;
			}

			if (placement.MBP_flags & MBP::top) {
				p.y = rel_y;
			}

			if (placement.MBP_flags & MBP::bottom) {
				p.y = rel_y + rel_h - pcs->cy;
			}

			if (placement.MBP_flags & MBP::left) {
				p.x = rel_x;
			}

			if (placement.MBP_flags & MBP::right) {
				p.x = rel_x + rel_w - pcs->cx;
			}

			//NOTE: this may or may not work, depends on the window, but I do think it always works for msgboxes
			pcs->x = p.x;
			pcs->y = p.y;

			//This didnt work
			//SetWindowPos(wnd, 0,p.x,p.y,0,0, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER);

			#if 1 //Add escape key functionality to MB_YESNO msgboxes (on esc key press the msgbox is canceled, returns IDNO)
			SetWindowSubclass(wnd,
				[](HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR , DWORD_PTR ) {
					//static int __cnt; printf("%d:MSGBOX:%s\n", __cnt++, msgToString(msg));
					switch (msg) {
					case WM_COMMAND:
					{
						auto notif = HIWORD(wparam);
						auto ctl_id = LOWORD(wparam);
						HWND ctl = (decltype(ctl))lparam;
						if (ctl_id == IDCANCEL) {
							LONG_PTR style = GetWindowLongPtr(wnd, GWL_STYLE);
							if (style & MB_YESNO) //TODO(fran): this check isnt quite correct, there are other styles that overlap with this one
								PostMessage(wnd, WM_COMMAND, MAKEWPARAM(IDNO, BN_CLICKED), 0); //thanks https://stackoverflow.com/questions/13872514/how-to-close-a-messagebox-window-by-its-handle-with-c
						}
					} break;
					}
					return DefSubclassProc(wnd, msg, wparam, lparam);
				}
			, 111, 0);
			#endif
		}
	}

	return CallNextHookEx(0 /*NOTE: this param was necessary on the times of win95, not anymore*/, code, wparam, lparam);
}

//INFO: placement is relative to hWnd
static int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType, int MBP_flags) {
	__msgbox_store_placement(hWnd, MBP_flags);
	HHOOK hook_proc = SetWindowsHookEx(WH_CBT, Hook_MsgBox, 0, GetCurrentThreadId()); //attach the hook
	int res = MessageBoxA(hWnd, lpText, lpCaption, uType);
	UnhookWindowsHookEx(hook_proc); // remove the hook
	return res;
}
//INFO: placement is relative to hWnd
static int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType, int MBP_flags) {
	__msgbox_store_placement(hWnd, MBP_flags);
	HHOOK hook_proc = SetWindowsHookEx(WH_CBT, Hook_MsgBox, 0, GetCurrentThreadId()); //attach the hook
	int res = MessageBoxW(hWnd, lpText, lpCaption, uType);
	UnhookWindowsHookEx(hook_proc); // remove the hook
	return res;
}

//----------------------FONT-----------------------:
#include <vector>		//TODO(fran): get rid of
static str GetApp_FontFaceName() {
	//Font guidelines: https://docs.microsoft.com/en-us/windows/win32/uxguide/vis-fonts
	//Stock fonts: https://docs.microsoft.com/en-us/windows/win32/gdi/using-a-stock-font-to-draw-text

	//TODO(fran): can we take the best codepage from each font and create our own? (look at font linking & font fallback)

	//We looked at 2195 fonts, this is what's left
	//Options:
	//Segoe UI (good txt, jp ok) (10 codepages) (supported on most versions of windows)
	//-Arial Unicode MS (good text; good jp) (33 codepages) (doesnt come with windows)
	//-Microsoft YaHei or UI (look the same,good txt,good jp) (6 codepages) (supported on most versions of windows)
	//-Microsoft JhengHei or UI (look the same,good txt,ok jp) (3 codepages) (supported on most versions of windows)

	HDC dc = GetDC(GetDesktopWindow()); defer{ ReleaseDC(GetDesktopWindow(),dc); }; //You can use any hdc, but not NULL
	std::vector<str> fontnames;
	EnumFontFamiliesExW(dc, NULL
		, [](const LOGFONTW* lpelfe, const TEXTMETRIC* /*lpntme*/, DWORD /*FontType*/, LPARAM lParam)->int {((std::vector<str>*)lParam)->push_back(lpelfe->lfFaceName); return TRUE; }
	, (LPARAM)&fontnames, NULL);

	const cstr* requested_fontname[] = { L"Segoe UI", L"Arial Unicode MS", L"Microsoft YaHei", L"Microsoft YaHei UI", L"Microsoft JhengHei", L"Microsoft JhengHei UI" };

	//for (int i = 0; i < ARRAYSIZE(requested_fontname); i++)
		//if (std::any_of(fontnames.begin(), fontnames.end(), [f = requested_fontname[i]](str s) {return !s.compare(f); })) return requested_fontname[i];

	//TODO(fran): faster search, we know the fontnames are ordered alphabetically
	for (int i = 0; i < ARRAYSIZE(requested_fontname); i++)
		for(const auto& f : fontnames) 
			if(!f.compare(requested_fontname[i])) return requested_fontname[i]; 

	return L"";
}

static SIZE avg_str_dim(HFONT font, size_t char_cnt) {
	SIZE res;

	HDC dc = GetDC(0); defer{ ReleaseDC(0,dc); };//TODO(fran): maybe it's better to generate a new dc, idk how reliable and non obtrusive this is since that dc is accessed by mutliple threads
	HFONT old_font = (HFONT)SelectObject(dc, font); defer{ SelectObject(dc,old_font); };//TODO(fran): pretty shitty solution
	TEXTMETRIC tm; GetTextMetrics(dc, &tm);
	res.cy = tm.tmHeight;
	res.cx = (int)(tm.tmAveCharWidth * char_cnt);

	return res;
}

//----------------------WPARAM-----------------------:
static WPARAM f32_to_WPARAM(f32 f) {
	int n; static_assert(sizeof(n) == sizeof(f), "Say whaat?!");
	memcpy(&n, &f, sizeof(f));
	return (WPARAM)n;
}

static f32 f32_from_WPARAM(WPARAM w) {
	f32 f;
	int n = (int)w;
	memcpy(&f, &n, sizeof(n));
	return f;
}


//----------------------HMONITOR-----------------------:
static u32 win32_get_refresh_rate_hz(HWND wnd) {
	//TODO(fran): this may be simpler with GetDeviceCaps
	u32 res = 60;
	HMONITOR mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST); //TODO(fran): should it directly receive the hmonitor?
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

//----------------------CARET-----------------------:
BOOL SetCaretPos(POINT p) {
	BOOL res = SetCaretPos(p.x, p.y);
	return res;
}


//----------------------GRID-----------------------:
#include <array> //create_grid_2x2

//returns a 2x2 configuration or 4x1 if there isnt enough max_w
	//w is used for horizontal centering only
	//TODO(fran): maybe creating a struct with 4 rect_i32 is less compiler expensive and less c++isy
	//NOTE: we dont handle the case when there's not enough height since it'll look too long if all the things are next to each other and you probably wont have the space anyway
std::array<std::array<rect_i32, 2>, 2> create_grid_2x2(i32 cell_h, i32 cell_w, i32 grid_y, i32 grid_w_pad, i32 grid_h_pad, i32 max_w, i32 w) {
	std::array<std::array<rect_i32, 2>, 2> res;
	for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) { res[i][j].w = cell_w; res[i][j].h = cell_h; }
	bool two_by_two = (cell_w * 2 + grid_w_pad) <= max_w;

	if (two_by_two) {
		res[0][0].left = w / 2 - grid_w_pad / 2 - res[0][0].w;	res[0][0].top = grid_y;
		res[0][1].left = w / 2 + grid_w_pad / 2;					res[0][1].top = res[0][0].top;

		res[1][0].left = res[0][0].left;						res[1][0].top = res[0][0].bottom() + grid_h_pad;
		res[1][1].left = res[0][1].left;						res[1][1].top = res[1][0].top;
	}
	else {
		for (int next_y = grid_y, i = 0; i < 2; i++)
			for (int j = 0; j < 2; j++) {
				res[i][j].left = (w - res[i][j].w) / 2;
				res[i][j].top = next_y;
				next_y = res[i][j].bottom() + grid_h_pad;
			}
	}
	return res;
}

//----------------------UINT (msg)-----------------------:
void print_string_msg(UINT msg) {
	if (msg >= 0xC000 && msg <= 0xFFFF) {//String messages for use by applications  
		TCHAR arr[256];
		int res = GetClipboardFormatName(msg, arr, 256);
		cstr_printf(arr); printf("\n");
		//After Alt+Shift to change the keyboard (and some WM_IMENOTIFY) we receive "MSIMEQueryPosition"
	}
}