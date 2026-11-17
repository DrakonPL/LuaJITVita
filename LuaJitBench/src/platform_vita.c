#include "platform.h"

#include "third_party/debugScreen.h"

#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <stdarg.h>

// Newlib takes this heap before main, as a game does. The vitasdk default is much larger.
#if defined(BIG_HEAP) && defined(NEAR_HEAP)
#error "BIG_HEAP and NEAR_HEAP cannot be used together"
#elif defined(BIG_HEAP)
int _newlib_heap_size_user = 128 * 1024 * 1024;
static const char *const kBaseHeapName = "bigheap";
#elif defined(NEAR_HEAP)
// Small enough that the first VM block is in the bl range of the VM code.
int _newlib_heap_size_user = 8 * 1024 * 1024;
static const char *const kBaseHeapName = "near";
#else
int _newlib_heap_size_user = 32 * 1024 * 1024;
static const char *const kBaseHeapName = "small";
#endif

enum {
    kHeapBlockSize = 128 * 1024 * 1024,
    kPollDelayUs = 16 * 1000,
    kLogLineMax = 256,
};

static const char *const kResultsDir = "ux0:data/LuaJitBench";

static SceUID s_heap_block = -1;
static SceUID s_arena_block = -1;
static void *s_arena;
static uint32_t s_prev_buttons;

void plat_init(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    psvDebugScreenInit();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
}

void plat_shutdown(void)
{
    plat_heap_block_free();
    sceKernelExitProcess(0);
}

uint64_t plat_time_us(void)
{
    return sceKernelGetProcessTimeWide();
}

void plat_log(const char *fmt, ...)
{
    char line[kLogLineMax];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof line, fmt, args);
    va_end(args);
    psvDebugScreenPuts(line);
    psvDebugScreenPuts("\n");
}

void plat_clear_screen(void)
{
    psvDebugScreenPuts("\e[H\e[2J");
}

PlatAction plat_next_action(void)
{
    for (;;) {
        SceCtrlData pad = { 0 };
        sceCtrlPeekBufferPositive(0, &pad, 1);
        const uint32_t pressed = pad.buttons & ~s_prev_buttons;
        s_prev_buttons = pad.buttons;
        if (pressed & SCE_CTRL_CROSS)
            return PLAT_ACTION_RUN;
        if (pressed & SCE_CTRL_SQUARE)
            return PLAT_ACTION_TOGGLE_HEAP_BLOCK;
        if (pressed & SCE_CTRL_TRIANGLE)
            return PLAT_ACTION_FREE_TEST;
        if (pressed & SCE_CTRL_CIRCLE)
            return PLAT_ACTION_FREE_FOR_VM_TEST;
        if (pressed & SCE_CTRL_START)
            return PLAT_ACTION_QUIT;
        sceKernelDelayThread(kPollDelayUs);
    }
}

const char *plat_results_dir(void)
{
    sceIoMkdir(kResultsDir, 0777); // fails when the folder exists
    return kResultsDir;
}

const char *plat_package_path(void)
{
    return "app0:scripts/?.lua;app0:?.lua";
}

int plat_cpu_mhz(void)
{
    return scePowerGetArmClockFrequency();
}

const char *plat_base_heap_name(void)
{
    return kBaseHeapName;
}

bool plat_has_heap_block(void)
{
    return true;
}

bool plat_heap_block_take(void)
{
    if (s_heap_block >= 0)
        return true;
    s_heap_block = sceKernelAllocMemBlock("ljb_heap_block", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, kHeapBlockSize, NULL);
    if (s_heap_block < 0) {
        plat_log("heap block: alloc of %d MB FAILED: 0x%08X", kHeapBlockSize / (1024 * 1024), (unsigned)s_heap_block);
        return false;
    }
    return true;
}

void plat_heap_block_free(void)
{
    if (s_heap_block < 0)
        return;
    sceKernelFreeMemBlock(s_heap_block);
    s_heap_block = -1;
}

void *plat_arena(size_t size)
{
    if (s_arena)
        return s_arena;
    const SceSize rounded = (SceSize)((size + 0xFFFu) & ~(size_t)0xFFFu); // USER_RW blocks are 4 KB pages
    s_arena_block = sceKernelAllocMemBlock("ljb_arena", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, rounded, NULL);
    if (s_arena_block < 0) {
        plat_log("arena: alloc of %u KB FAILED: 0x%08X", (unsigned)(rounded / 1024), (unsigned)s_arena_block);
        return NULL;
    }
    sceKernelGetMemBlockBase(s_arena_block, &s_arena);
    return s_arena;
}
