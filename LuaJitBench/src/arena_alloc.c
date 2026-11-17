#include "arena_alloc.h"

#include "third_party/tlsf/tlsf.h"

#include <stdint.h>
#include <string.h>

// LuaJIT expects 8-byte aligned memory, as from its own allocator. TLSF aligns to 4 bytes on 32-bit.
enum { kAlignment = 8 };

static void *arena_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    tlsf_t tlsf = (tlsf_t)ud;
    if (nsize == 0) {
        tlsf_free(tlsf, ptr);
        return NULL;
    }
    if (!ptr)
        return tlsf_memalign(tlsf, kAlignment, nsize);
    if (nsize <= osize)
        return tlsf_realloc(tlsf, ptr, nsize); // shrinks in place, so the address stays aligned

    // tlsf_realloc can move the block to a 4-byte aligned address. So a growing block gets a new aligned block.
    void *grown = tlsf_memalign(tlsf, kAlignment, nsize);
    if (!grown)
        return NULL;
    memcpy(grown, ptr, osize);
    tlsf_free(tlsf, ptr);
    return grown;
}

lua_State *arena_newstate(void *mem, size_t size)
{
    tlsf_t tlsf = tlsf_create_with_pool(mem, size);
    if (!tlsf)
        return NULL;
    return lua_newstate(arena_lua_alloc, tlsf);
}
