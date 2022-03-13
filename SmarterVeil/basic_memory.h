#pragma once

struct memory_arena {
    u8* base;
    u32 sz;
    u32 used;
};

internal void initialize_arena(memory_arena* arena, u8* mem, u32 sz) {
    arena->base = mem;
    arena->sz = sz;
    arena->used = 0;
}

internal void* _push_mem(memory_arena* arena, u32 sz) {
    assert((arena->used + sz) <= arena->sz);//TODO(fran): < or <= ?
    void* res = arena->base + arena->used;
    arena->used += sz;
    return res;
} //TODO(fran): set mem to zero?

#define push_type(arena,type) (type*)_push_mem(arena,sizeof(type))

#define push_arr(arena,type,count) (type*)_push_mem(arena,sizeof(type)*count)

#define push_sz(arena,sz) _push_mem(arena,sz)

#define zero_mem(address,sz) memset((address),0,(sz)) /*TODO(fran): optimize*/
#define zero_struct(instance) zero_mem(&(instance),sizeof(instance))