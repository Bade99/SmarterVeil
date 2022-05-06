#pragma once

template<typename T>
struct fixed_array_header {
    u64 cnt;
    T* arr;

    T* begin()
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    T* end()
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

    const T* begin() const
    {
        return this->cnt ? &arr[0] : nullptr;
    }

    const T* end() const
    {
        return this->cnt ? &arr[0] + this->cnt : nullptr;
    }

};
struct ui_element;
template<typename T, u64 _cnt>
struct fixed_array { //TODO(fran): move to basic_array.h
    u64 cnt; //cnt in use / cnt_used
    T arr[_cnt];

    constexpr u64 cnt_allocd()
    {
        u64 res = ArrayCount(this->arr);
        return res;
    }

    T& operator[] (u64 idx)
    {
        return this->arr[idx];
    }

    const T& operator[] (u64 idx) const
    {
        return this->arr[idx];
    }

    void clear() { this->cnt = 0; zero_struct(this->arr); }

#if 1
    fixed_array<T, _cnt>& operator+=(T e)//TODO(fran): see if this is a good idea
    {
        if (this->cnt + 1 > this->cnt_allocd()) crash();
        this->arr[this->cnt++] = e;
        return *this;
    }

    fixed_array<T, _cnt>& add(T e)
    {
        return *this += e;
    }
#endif

    fixed_array<T, _cnt>& remove_idx(u64 idx)
    {
        assert(idx < this->cnt);

        if (this->cnt) this->cnt -= 1;

        this[idx] = this[this->cnt];

        return this; //TODO(fran): we may want to return an iterator to the next element? aka this[idx]
    }

    fixed_array<T, _cnt>(std::initializer_list<T> elems)
    {
        this->cnt = 0;
        assert(elems.size() <= this->cnt_allocd());
        for (auto& e : elems) this->arr[this->cnt++] = e;
    }

#if 0
    T* begin() { return this->cnt ? &arr[0] : nullptr; }
    T* end() { return this->cnt ? &arr[0] + this->cnt : nullptr; }
    const T* begin() const { return this->cnt ? &arr[0] : nullptr; }
    const T* end() const { return this->cnt ? &arr[0] + this->cnt : nullptr; }

    T* rbegin() { auto end = this->end(); return end ? --end : nullptr; }
    T* rend() { auto begin = this->begin(); return begin ? --begin : nullptr; }
    const T* rbegin() const { auto end = this->end(); return end ? --end : nullptr; }
    const T* rend() const { auto begin = this->begin(); return begin ? --begin : nullptr; }
#else
    T* begin() { return &arr[0]; }
    const T* begin() const { return &arr[0]; }
    T* end() { return &arr[0] + this->cnt; }
    const T* end() const { return &arr[0] + this->cnt; }

    std::reverse_iterator<T*> rbegin() { return std::reverse_iterator(this->end()); }
    std::reverse_iterator<T*> rend() { return std::reverse_iterator(this->begin()); }
    std::reverse_iterator<const T*> rbegin() const { return std::reverse_iterator(this->end()); } //TODO(fran): not sure this is how you make const iterators
    std::reverse_iterator<const T*> rend() const { return std::reverse_iterator(this->begin()); }
#endif

    operator fixed_array_header<T>() { return { this->cnt, this->arr }; }
};