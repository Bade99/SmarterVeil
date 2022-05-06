#pragma once

internal i64 StartCounter()
{
    i64 res = OS::StartCounter();
    return res;
}
internal f64 EndCounter(i64 CounterStart, OS::counter_timescale timescale = OS::counter_timescale::milliseconds)
{
    f64 res = OS::EndCounter(CounterStart, timescale);
    return res;
}

//TODO(fran): stuff below here may belong in a advanced_timing.h

struct timed_element { s8 name; f32 t; u32 cnt; /*b32 is_function_or_block;*/ };

namespace std {
    template <> struct hash<timed_element>
    {
        size_t operator()(const timed_element& x) const
        {
            return std::hash<std::string_view>()(std::string_view((char*)x.name.chars, x.name.cnt));
        }
    };
}

#define is_powerof_2(n) ((n & (n - 1)) == 0)

template<typename T> b32 compare(const T& a, const T& b); //returns true or false

template<> b32 compare(const timed_element& a, const timed_element& b)
{
    b32 res = strcmp((char*)a.name.chars, (char*)b.name.chars)==0; //TODO(fran): pass in both char counts too so we do not require null termination
    return res;
}

template<typename T, u64 table_cnt, u64 extra_entries_cnt>
struct hash_table { //NOTE(fran): requires zero initialized memory
    static_assert(is_powerof_2(table_cnt), "table_cnt must be set to a power of 2");

    struct hash_table_element {
        T val; //NOTE(fran): the key will be contained inside the val, for now
        hash_table_element* next_in_hash; //NOTE(fran): for collisions we create a linked list
    } *table[table_cnt]; 
    hash_table_element table_entries[table_cnt + extra_entries_cnt];
    u32 table_entries_cnt;

    using value_type = T;// decltype(remove_reference<decltype(*table[0])>::type::val);

    u32 get_hash(const value_type& val)
    {
        u32 res = std::hash<value_type>()(val) & (ArrayCount(this->table) - 1);
        return res;
    }

    value_type* AddEntry(const value_type& val)
    {
        value_type* res{};
        hash_table_element* found = nil;
        u32 hash_bucket = this->get_hash(val);
        for (hash_table_element* e = this->table[hash_bucket]; e; e = e->next_in_hash)
        {
            if (compare(e->val, val))
            {
                //OutputDebugStringA("[ERROR] Duplicated language entry key");
                found = e; //TODO(fran): for this case if found I think we want to add to t and cnt++
                break;
            }
        }
        if (!found) {
            assert(this->table_entries_cnt < ArrayCount(this->table_entries));
            found = &this->table_entries[this->table_entries_cnt++];
            found->next_in_hash = this->table[hash_bucket];
            this->table[hash_bucket] = found;
        }
        if (found) {
            found->val = val;
            res = &found->val;
        }
        return res;
    }

    value_type* GetEntry(const value_type& val)
    {
        value_type* res{ 0 };

        u32 hash_bucket = this->get_hash(val);

        for (hash_table_element* e = this->table[hash_bucket]; e; e = e->next_in_hash)
        {
            if (compare(e->val, val))
            {
                res = &e->val;
                break;
            }
        }
        return res;
    }
};

struct time_table {
    hash_table<timed_element,256,128> table;
    fixed_array<timed_element*, 256 + 128> fast_table;

    void AddOrModify(const timed_element& val)
    {
        if (auto te = this->table.GetEntry(val))
        {
            te->cnt++;
            te->t += val.t;
        }
        else
        {
            auto new_te = this->table.AddEntry(val);
            this->fast_table.add(new_te);
        }
    }
};

time_table _TIMETABLETEST[2]{};
time_table* TIMETABLETEST= &_TIMETABLETEST[0];
time_table* OLDTIMETABLETEST = &_TIMETABLETEST[1];