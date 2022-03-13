#pragma once

#include <string> // strnicmp
#include <stdlib.h> //strtoul

//TODO(fran): add entry referencing, eg entry 199 = "hello \200", entry 200 = "fran" -> entry 199 = "hello fran". NOTE: we should solve entry referencing after parsing all entries, since one could point to a later, not yet parsed, entry. NOTE: add a recursion limit, otherwise this can overflow the stack

//Up to now we've been using the resource files (.rc) to store languages, this is annoying for a couple of reasons, that's why this file will define a new way to manage languages:
//there will be a couple of default languages that are stored with the .exe similar to how .rc does it, we'll manually set up some map/array whatever to write the strings we need and their mapping, and on startup those will be sent to disc on some specified folder, once that's done the language manager will read those files, and any other in the same folder, from disk, it'll either load only the current language and go to disk each time it needs to swap, or it'll load everything and just switch in code
//with this we get rid of the language enums and new languages can be very easily added after the fact

//IMPORTANT: Format of the file on disk: 
//	the file name will identify the language, meaning the user will be shown that same string, eg: c\user\appdata\app\lang\Español -> Español
//	file encoding must be UTF-8
//	inside the file there will be one language entry on each line
//		line format: number   tab/separator   string
//	additionally to support things like line breaks we parse the string to find the character representation of \n, \0, \t, ... and convert it to the single character it wants to represent, eg L"this is a \\n test" -> parser -> L"this is a \n test"

struct language { //Identifies a default language that will be stored to disk by the application
	//TODO(fran): having the description in the file seems unnecessary, we should have a big language codes table with id->description mapping
	const utf8* entries;
	u64 entries_sz;//bytes //TODO(fran): change to cnt, or use s8

	const s8 get_code() const //ISO Language code
	{
		if (entries && entries_sz)
		{
			s8 data{ .chars = const_cast<utf8*>(entries), .cnt = entries_sz / sizeof(*entries) };
			auto idx = data.find_next('\n', 0); //go past the id
			if (idx != U64MAX)
			{
				auto start = 0;
				auto end = idx;

				//TODO(fran): check for valid language code

				return s8{ .chars = data.chars + start, .cnt = end - start };
			}
			else
			{
				OutputDebugStringA("[ERROR] The language does not contain its ISO Language Code\n"); //TODO(fran): notify the user (msgbox)
				//TODO(fran): some way to be more specific about which language failed?
				DebugBreak();
			}
		}
		return s8{0};
	}
	const s8 get_description() const
	{
		if (entries && entries_sz)
		{
			s8 data{ .chars = const_cast<utf8*>(entries), .cnt = entries_sz / sizeof(*entries) };
			auto idx = data.find_next('\n', 0); //go past the id
			if (idx != U64MAX)
			{
				auto start = idx+1;
				idx = data.find_next('\n', idx+1); //go past the description

				if (idx != U64MAX)
				{
					auto end = idx;
					return s8{ .chars = data.chars + start, .cnt = end - start };
				}
			}
			OutputDebugStringA("[ERROR] The language does not contain its Language Description\n");
			DebugBreak();
		}
		return s8{ 0 };
	}
	const s8 get_version() const
	{
		if (entries && entries_sz)
		{
			s8 data{ .chars = const_cast<utf8*>(entries), .cnt = entries_sz / sizeof(*entries) };
			auto idx = data.find_next('\n', 0); //go past the id
			if (idx != U64MAX)
			{
				idx = data.find_next('\n', idx + 1); //go past the description

				if (idx != U64MAX)
				{
					auto start = idx + 1;

					idx = data.find_next('\n', idx + 1); //go past the version

					if (idx != U64MAX)
					{
						auto end = idx;
						return s8{ .chars = data.chars + start, .cnt = end - start };
					}
				}
			}
			OutputDebugStringA("[ERROR] The language does not contain a Version Number\n");
			DebugBreak();
		}
		return s8{ 0 };
	}
	const s8 get_start_of_entries() const
	{
		if (entries && entries_sz)
		{
			s8 data{ .chars = const_cast<utf8*>(entries), .cnt = entries_sz / sizeof(*entries) };
			auto idx = data.find_next('\n', 0); //go past the id
			if (idx != U64MAX)
			{
				idx = data.find_next('\n', idx + 1); //go past the description

				if (idx != U64MAX)
				{

					idx = data.find_next('\n', idx + 1); //go past the version

					if (idx != U64MAX)
					{
						auto start = idx + 1;
						auto end = data.cnt;
						if(end>start)
							return { .chars = data.chars + start, .cnt = end - start };
					}
				}
			}
			OutputDebugStringA("[ERROR] The language does not contain Entries\n");
			DebugBreak();
		}
		return { 0 };
	}
};

#define lang_key_value_separator u8##" "
/*IMPORTANT TODO(fran): japanese, and maybe other langs, map the space key to a different character "　" instead of " ", we gotta fix this somehow, option 1 provide a separate application "lang editor" for the user to create languages in, that takes care of this problems, option 2 have a better parser that takes this into account*/
#define lang_newline u8##"\n"

#define lang_make_encoding(quote) u8##quote

#define lang_make_text(quote) lang_make_encoding(#quote)

#define lang_entry(id,txt) lang_make_text(id) lang_key_value_separator txt lang_newline

#define lang_code(id) id lang_newline
#define lang_desc(desc) desc lang_newline
#define lang_version(v) v lang_newline

//NOTE(fran): lang_entry 0 should never be used, in order to preserve the concept of a null value. In which case we return an 'empty' string ""

//TODO(fran): we probably want to use separate files that we straight include with each of this
constexpr static utf8 lang_english_entries[] =
lang_code("en-us")
lang_desc("English (US)")
lang_version("1")
lang_entry(1, "Restore")
lang_entry(2, "Minimize")
lang_entry(3, "Maximize")
lang_entry(4, "Close")

lang_entry(50, "Turn On")
lang_entry(51, "Turn Off")
lang_entry(52, "Define a hotkey to open this window") //TODO(fran): does macos & linux also call it window?
;

constexpr static utf8 lang_español_entries[] =
lang_code("es-ar")
lang_desc("Español (Argentina)")
lang_version("1")
lang_entry(1, "Restaurar")
lang_entry(2, "Minimizar")
lang_entry(3, "Maximizar")
lang_entry(4, "Cerrar")

lang_entry(50, "Encender")
lang_entry(51, "Apagar")
lang_entry(52, "Define un atajo para abrir esta ventana")
;

internal void save_languages_to_disc(language* langs, i32 cnt, const s8 folder) {
	for (i32 i = 0; i < cnt; i++)
	{
		language lang = langs[i];
		s8 code = lang.get_code(); assert(code.chars);
		s8 desc = lang.get_description(); assert(desc.chars);
		s8 version = lang.get_version(); assert(version.chars);

		s8 filename = folder.create_copy(malloc, desc.cnt);
		defer{ free(filename.chars); };
		filename += desc;

		b32 write_file = true;

		auto old_file = OS::read_entire_file(filename); defer{ OS::free_file_memory(old_file.mem); };

		if (old_file.mem)
		{
			language old_lang = { .entries = (utf8*)old_file.mem, .entries_sz = old_file.sz };

			auto old_ver = old_lang.get_version();
			//TODO(fran): find a string to int function that accepts the string's cnt
			utf8 v[16]{ 0 }; memcpy(v, old_ver.chars, minimum(old_ver.cnt, ArrayCount(v) - 1));
			auto old_version = strtoul((char*)v, nil, 10);
			auto new_version = strtoul((char*)version.chars, nil, 10);
			write_file = new_version > old_version; //TODO(fran): compare with != or > ?

        #ifdef DEBUG_BUILD
            write_file = true;
        #endif
		}

		if (write_file)
		{
			u32 sz = lang.entries_sz - (lang.entries[(lang.entries_sz / sizeof(*lang.entries)) - 1] ? 0 : sizeof(*lang.entries));//NOTE(fran): remove null terminator at the time of saving the file
			OS::write_entire_file(filename, (void*)lang.entries, sz);
		}
	}
}

//In order to change languages the system will go look for the specific language from disc, this will not be as fast as loading all the languages on startup but will potentially reduce startup time by a lot since only one language needs to be entirely loaded and parsed; changing languages realtime will also be not as fast but the user isnt usually changing languages so that isnt a real concern

//TODO(fran): take care of the \r \n \r\n stupidity

struct language_manager {
    utf8 lang_folder[8];

    language available_languages[256];
    u32 available_languages_cnt;

    u32 current_language;
    struct string_mapping {
        u32 key;
        s8 val;
        string_mapping* next_in_hash;
    } *string_table_hash[256]; //NOTE: must be a power of 2

    u8 _temp_string_arena[1024*sizeof(utf8)]; //TODO(fran): we may want to have real allocation
    memory_arena temp_string_arena;

    u8 _string_mapping_arena[64*sizeof(string_mapping)]; //TODO(fran): we may want to have real allocation
    memory_arena string_mapping_arena;

    u32 get_language_idx(s8 ISO_language_code)
    {
        for (u32 i = 0; i < available_languages_cnt; i++)
        {
            auto a = ISO_language_code.chars;
            auto b = available_languages[i].get_code().chars;
            auto cnt = minimum(ISO_language_code.cnt, available_languages[i].get_code().cnt);
            if (strnicmp((char*)a, (char*)b, cnt) == 0) return i;
        }
        return U32MAX;
    }

    void add_string_mapping(u32 key, s8 val)
    {
        string_mapping* found = nil;
        u32 hash_bucket = this->get_hash(key);
        for (string_mapping* mapping = this->string_table_hash[hash_bucket]; mapping; mapping = mapping->next_in_hash)
        {
            if (mapping->key == key)
            {
                OutputDebugStringA("[ERROR] Duplicated language entry key");
                found = mapping;
                break;
            }
        }
        if (!found) {
            found = push_type(&this->string_mapping_arena, string_mapping);
            found->next_in_hash = this->string_table_hash[hash_bucket];
            this->string_table_hash[hash_bucket] = found;
        }
        if (found) {
            found->key = key;
            found->val = val;
        }
    }

    void create_language_table()
    {
        zero_struct(string_table_hash);

        this->add_string_mapping(0, {0});

        utf8 separator = lang_key_value_separator[0];
        s8 data = this->available_languages[this->current_language].get_start_of_entries();

        //TODO(fran): entry parsing, to add \n, \t, etc support, or change the structure of the entry, make it be a number on one line and then n text lines that can contain a real \n

        u64 start = 0;
        u64 next_start = 0;
        while (true) {
            next_start = data.find_next(u8'\n', start); //TODO(fran): this requires the final line to also contain an end of line, we may or may not want to change that
            if (next_start == U64MAX) break;
            s8 line = { .chars = data.chars + start, .cnt = next_start - start };

            s8 key, val;
            
            start = line.skip_whitespace(0);
            if (start != U64MAX) //TODO(fran): reduce the amount of ifs
            {
                u64 end = line.find_next(separator, start);
                if (end != U64MAX)
                {
                    key = { .chars = line.chars + start, .cnt = end - start };

                    start = line.skip_whitespace(end + 1);

                    if (start != U64MAX)
                    {
                        end = line.cnt;

                        //TODO(fran): reverse_skip_whitespace() from end?

                        val = { .chars = line.chars + start, .cnt = end - start };

                        utf8 key_str[16]{ 0 };
                        memcpy(key_str, key.chars, minimum(key.cnt * sizeof(key[0]), (ArrayCount(key_str) - 1) * sizeof(key_str[0])));
                        //TODO(fran): make our own function that takes string cnt & returns struct{b32 found, u32 val}
                        if(u32 key_res = strtoul((char*)key_str,nil,10); key_res != 0 && key_res != U32MAX)
                            this->add_string_mapping(key_res, val);
                    }
                }
            }
            start = next_start + 1;
        }
    }

    void load_languages_from_disc() {
        using namespace std::filesystem;
        for (auto& f : directory_iterator(lang_folder)) { //TODO(fran): performance: do this with FindFirstFile and FindNextFile, problem is I have to manually check we dont enter into subdirs. Also this is only available on c++17
            if (f.is_regular_file()) {
                if (ArrayCount(available_languages) <= (available_languages_cnt))
                {
                    OutputDebugStringA("[ERROR] Too many languages to fit into the array\n");
                    DebugBreak();
                    break;
                }
                auto read_res = OS::read_entire_file({ .chars = f.path().u8string().data(), .cnt = f.path().u8string().length() });

                if (read_res.mem)
                {
                    available_languages[available_languages_cnt++] =
                    {
                        .entries = (utf8*)read_res.mem,
                        .entries_sz = read_res.sz,
                    };

                    if (s8{ (utf8*)read_res.mem, read_res.sz / sizeof(utf8) }.find_next('\r', 0) != U64MAX)
                    {
                        OutputDebugStringA("[ERROR] Incorrect line endings, please use Unix style (\\n)");
                        DebugBreak();
                    }
                }
            }
        }

        assert(this->available_languages_cnt > 0);

        auto sort_alphabetic = [](const void* l, const void* r) -> int
        {
            const language* ls = (decltype(ls))l;
            const language* rs = (decltype(rs))r;
            auto ls_desc = ls->get_description();
            auto rs_desc = rs->get_description();
            return strncmp((char*)ls_desc.chars, (char*)rs_desc.chars, minimum(ls_desc.cnt, rs_desc.cnt));
            //Sort A to Z, TODO(fran): check how well this sorts langs that dont use our alphabet
        };

        qsort(available_languages, available_languages_cnt, sizeof(available_languages[0]), sort_alphabetic);
    }

    u32 get_hash(u32 str_id)
    {
        //NOTE(fran): zero must map to zero
        //TODO(fran): better hash function
        u32 res = str_id & (ArrayCount(this->string_table_hash) - 1);
        return res;
    }

    struct get_string_res { s8 s; b32 found; };
    get_string_res _GetString(u32 str_id)
    {
        get_string_res res{0};

        u32 hash_bucket = this->get_hash(str_id);

        for (string_mapping* mapping = this->string_table_hash[hash_bucket]; mapping; mapping = mapping->next_in_hash) {
            if (mapping->key == str_id)
            {
                res.found = true;
                res.s = mapping->val;
                break;
            }
        }
        return res;
    }

    b32 ChangeLanguage(s8 ISO_language_code)
    {
        b32 res = false;
        auto idx = get_language_idx(ISO_language_code);
        res = idx != U32MAX;
        if (res)
        {
            this->current_language = idx;
            this->create_language_table();
        }
        else {} //TODO(fran): substring comparisons to find closest possible language
        return res;
    }

    s8 GetString(u32 str_id)
    {
        s8 res;
        auto search = _GetString(str_id);
        if (search.found) res = search.s;
        else
        {
            utf8 _not_found[] = u8"#%u";
            u32 cnt_allocd = ArrayCount(_not_found) + 16;
            //TODO(fran): reset string_arena's used counter when it gets filled up
            res.chars = push_arr(&this->temp_string_arena,utf8, cnt_allocd);
            res.cnt_allocd = cnt_allocd;
            res.cnt = sprintf_s((char*)res.chars, res.cnt_allocd, (char*)_not_found, str_id);
        }
        return res;
    }
};