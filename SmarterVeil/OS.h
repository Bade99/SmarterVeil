#pragma once

#include <shlobj_core.h> // SHGetKnownFolderPath

#if defined(_WIN32) || defined(_WIN64)
#define OS_WINDOWS
#else
#error Define your Operating System
#endif

struct hotkey_data {
#ifdef OS_WINDOWS
	u16 mods; //Hotkey modifiers: ctrl, alt, shift
	u8 vk;    //Virtual Key
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
#else
#error Define your OS's implementation
#endif
};

//NOTE(fran): helpers for the OS itself, should not be used by outside callers
namespace _OS
{
	//NOTE(fran): small memory would be anything that will be generally below the OS's page size
	internal void* alloc_small_mem(u64 sz)
	{
#ifdef OS_WINDOWS
		return malloc(sz);
#else
#error Define your OS's implementation
#endif
	}

	internal void free_small_mem(void* mem)
	{
#ifdef OS_WINDOWS
		free(mem);
#else
#error Define your OS's implementation
#endif
	}

	internal s16 convert_to_s16(const s8 s)
	{
#ifdef OS_WINDOWS
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
#else
#error Define your OS's implementation
#endif
	}

	internal s8 convert_to_s8(const s16 s)
	{
#ifdef OS_WINDOWS
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
#else
#error Define your OS's implementation
#endif
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
#ifdef OS_WINDOWS

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
		u64 next_dir=0;
		u64 last_dir = find_prev_slash(filename, filename.cnt-1);
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

#endif
	}

	internal s8 get_work_folder() {
	   	constexpr utf8 app_folder[] = appname u8"/";

#ifdef OS_WINDOWS
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
#else
#error Define your OS's implementation
#endif
	}

	//TODO(fran): for this OS internal kinds of things we should have the string type change with OS
	internal b32 windows_set_working_directory(s16 dir) {
#ifdef OS_WINDOWS
		b32 res = SetCurrentDirectoryW((wchar_t*)dir.chars);
		return res;
#else
#error Define your OS's implementation
#endif
	}
}

//NOTE(fran): services provided by the OS to the application

//IMPORTANT: for filepaths use / instead of \\, since / should be supported by default in all OSs

namespace OS
{
	struct _work_folder{
		s8 dir_str;

		_work_folder(){
			dir_str = _OS::get_work_folder();

#ifdef OS_WINDOWS
			s16 work_dir16 = _OS::convert_to_s16(dir_str); defer{ _OS::free_small_mem(work_dir16.chars); };
			_OS::windows_create_subdirectories(work_dir16);
			b32 res = _OS::windows_set_working_directory(work_dir16);
			assert(res);
#else
#error Define your OS's implementation
#endif
		}

		~_work_folder() { _OS::free_small_mem(dir_str.chars); };
	}global_persistence work_folder;


	internal bool write_entire_file(const s8 filename, void* memory, u64 mem_sz) 
	{
#ifdef OS_WINDOWS
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
#else
#error Define your OS's implementation
#endif
	}

	void free_file_memory(void* memory) 
	{ //for read_entire_file_res
		if (memory) VirtualFree(memory, 0, MEM_RELEASE);
	}

	struct read_entire_file_res { void* mem; u64 sz; };
	read_entire_file_res read_entire_file(const s8 filename) 
	{
#ifdef OS_WINDOWS
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
#else
#error Define your OS's implementation
#endif
	}

#ifdef OS_WINDOWS
	#define locale_max_length (LOCALE_NAME_MAX_LENGTH*2) /*utf8 length*/
#else
#error Define your OS's implementation
#endif
	//returns true if successful, if res->cnt is at least equal to locale_max_length the function will never fail
	b32 get_user_locale(s8* res) 
	{
		assert(res->cnt >= locale_max_length);
#ifdef OS_WINDOWS
		utf16 loc16[LOCALE_NAME_MAX_LENGTH];
		b32 ret = GetUserDefaultLocaleName( (wchar_t*)loc16, ArrayCount(loc16));

		s8 loc = _OS::convert_to_s8(temp_s(loc16)); defer{ _OS::free_small_mem(loc.chars); };
		*res += loc;

		return ret;
#else
#error Define your OS's implementation
#endif
	}

	void HotkeyToString(hotkey_data hk, s8* hotkey_str)
	{
#ifdef OS_WINDOWS
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
		s8 vk_str{.chars= _vk_str, .cnt=0, .cnt_allocd=ArrayCount(_vk_str)};
		
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
#else
#error Define your OS's implementation
#endif
	}

}
