#pragma once

#define temp_s(c_string) {.chars = c_string, .cnt=s_cnt(c_string), .cnt_allocd=0}
//TODO(fran): HACK: had to fix it utf8 cause I could quite find the way to make a const_cast for any type
#define const_temp_s(c_string) {.chars = const_cast<utf8*>(c_string), .cnt=s_cnt(c_string), .cnt_allocd=0}

//TODO(fran): use the string as the type, eg s8, and retrieve its inner type (utf8) from there
#define stack_s(type, char_cnt){.chars=(type::value_type*)alloca(char_cnt*sizeof(type::value_type)), .cnt=0, .cnt_allocd=char_cnt}

template<typename T>
internal constexpr u64 s_cnt(const T* s)
{ //does not include null terminator
	u64 res{ 0 }; 
	while (*s++) res++;
	return res;
}

//NOTE(fran): this string will _never_ contain a null terminator while in the application code. Not the same inside OS code, where it's needed because certain OS's expect null termination. Therefore the only way to add it is by manual insertion.
template<typename T>
struct sN { 
	using value_type = T;

	T* chars;
	u64 cnt;
	u64 cnt_allocd;

	u64 sz() const 
	{
		return cnt * sizeof(*chars);
	}

	u64 sz_allocd() const 
	{
		return cnt_allocd * sizeof(*chars);
	}

	u64 find_next(T character, u64 from /*inclusive*/) const 
	{
		for(u64 i = from; i < cnt; i++)
		{
			if (chars[i] == character) return i;
		}
		return U64MAX;
	}

	//Returns the index of the first character that is not a whitespace
	u64 skip_whitespace(u64 from /*inclusive*/) const
	{
		for (u64 i = from; i < cnt; i++)
		{
			switch (chars[i])
			{
				case ' ':
				case '\t':
				case '\n':
				case '\v':
				case '\f':
				case '\r':
				//TODO(fran): add whitespaces from other languages, for example japanese has a long space which occupies multiple utf8 chars
					continue;
				default: 
					return i;
			}
		}
		return U64MAX;
	}

	//NOTE(fran): characters must match exactly, this function is case sensitive
	u64 find_next(sN<T> str, u64 from /*inclusive*/) const
	{
		for (u64 i = from; i < cnt; i++)
		{
			//TODO(fran): performance test
			b32 found = false;
			for (u64 j = 0; j < str.cnt; j++)
				if (chars[i + j] == str[j]) found = true;
				else { found = false; break; }
			if(found) return i;
		}
		return U64MAX;
	}

	u64 find_previous(T character, u64 from /*inclusive*/) const 
	{
		for (u64 i = from; i < cnt; i--)
		{
			if (chars[i] == character) return i;
			if(i == 0) return U64MAX;
		}
		return U64MAX;
	}

	T& operator[] (u64 idx) const
	{
		return chars[idx];
	}

	sN<T>& operator+=(const sN<T> s)
	{
		if (cnt + s.cnt > cnt_allocd) crash();
		for (u64 i = 0; i < s.cnt ; i++) 
		{
			chars[cnt++] = s.chars[i];
		}
		return *this;
	}

	sN<T>& operator+=(const T* s)
	{
		u64 add_cnt = s_cnt(s);
		if (cnt + add_cnt > cnt_allocd) crash();
		for (u64 i = 0; i < add_cnt ; i++)
		{
			chars[cnt++] = s[i];
		}
		return *this;
	}

	typedef void* allocator(u64);
	sN<T> create_copy(allocator alloc, u64 cnt_extra) const
	{
		sN<T> res =
		{
			.chars = (T*)alloc((cnt + cnt_extra) * sizeof(*chars)),
			.cnt = 0,
			.cnt_allocd = cnt + cnt_extra,
		};
		res += *this;
		return res;
	}

};

typedef sN<utf8> s8; //String holding 8bit wide characters, with utf8 encoding
typedef sN<utf16> s16; //String holding 16bit wide characters, with utf16 encoding
typedef sN<utf32> s32; //String holding 32bit wide characters, with utf32 encoding
