#ifndef LJB_ARENA_ALLOC_H
#define LJB_ARENA_ALLOC_H

#include <lua.h>
#include <stddef.h>

// A new Lua state whose allocator is a TLSF pool on [mem, mem + size), with no lock.
// The pool starts empty at each call, so the memory must not hold a live state.
lua_State *arena_newstate(void *mem, size_t size);

#endif
