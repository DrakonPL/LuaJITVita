#include "mcode_wrap.h"
#include "platform.h"

#include <psp2/kernel/sysmem.h>
#include <string.h>

// Defined in the LuaJIT VM (hidden visibility, resolved by the static link).
extern char lj_vm_exit_handler[];

SceUID __real_sceKernelAllocMemBlockForVM(const char *name, SceSize size);
int __real_sceKernelOpenVMDomain(void);
int __real_sceKernelCloseVMDomain(void);

enum { kMaxKeptBlocks = 64 }; // 8 code areas + 1 callback block, with room for the retries of the fork

typedef struct VmBlock {
    uintptr_t base;
    uint32_t size;
} VmBlock;

static VmBlock s_blocks[kMaxKeptBlocks];
static int s_count;
static int s_failures;
static int s_first_error;
static int s_last_error;
static uint32_t s_last_failed_size;
static int s_domain_failures;
static int s_domain_first_error;
static double s_max_distance_mb;

uintptr_t vm_exit_handler_address(void)
{
    return (uintptr_t)lj_vm_exit_handler;
}

static uintptr_t abs_diff(uintptr_t a, uintptr_t b)
{
    return a > b ? a - b : b - a;
}

double vm_block_distance_mb(uintptr_t base, uint32_t size)
{
    const uintptr_t target = vm_exit_handler_address();
    const uintptr_t to_base = abs_diff(target, base);
    const uintptr_t to_end = abs_diff(target, base + size);
    return (double)(to_base > to_end ? to_base : to_end) / (1024.0 * 1024.0);
}

SceUID __wrap_sceKernelAllocMemBlockForVM(const char *name, SceSize size)
{
    const SceUID uid = __real_sceKernelAllocMemBlockForVM(name, size);
    if (uid < 0) {
        if (s_failures == 0)
            s_first_error = uid;
        s_failures++;
        s_last_error = uid;
        s_last_failed_size = size;
        return uid;
    }

    void *base = NULL;
    if (sceKernelGetMemBlockBase(uid, &base) < 0)
        return uid;

    if (s_count < kMaxKeptBlocks)
        s_blocks[s_count] = (VmBlock){ (uintptr_t)base, size };
    s_count++;

    const double distance = vm_block_distance_mb((uintptr_t)base, size);
    if (distance > s_max_distance_mb)
        s_max_distance_mb = distance;
    return uid;
}

static int record_domain_result(int result)
{
    if (result < 0) {
        if (s_domain_failures == 0)
            s_domain_first_error = result;
        s_domain_failures++;
    }
    return result;
}

int __wrap_sceKernelOpenVMDomain(void)
{
    return record_domain_result(__real_sceKernelOpenVMDomain());
}

int __wrap_sceKernelCloseVMDomain(void)
{
    return record_domain_result(__real_sceKernelCloseVMDomain());
}

void plat_vm_blocks_reset(void)
{
    s_domain_failures = 0;
    s_domain_first_error = 0;
    s_count = 0;
    s_failures = 0;
    s_first_error = 0;
    s_last_error = 0;
    s_last_failed_size = 0;
    s_max_distance_mb = 0.0;
}

VmBlockStats plat_vm_blocks_stats(void)
{
    return (VmBlockStats){ s_count, s_failures, s_first_error, s_last_error, s_domain_failures, s_domain_first_error,
                           s_max_distance_mb, true };
}

void plat_vm_blocks_describe(char *out, size_t out_size)
{
    size_t used = 0;
    out[0] = '\0';
    const int kept = s_count < kMaxKeptBlocks ? s_count : kMaxKeptBlocks;
    for (int i = 0; i < kept && used < out_size; i++) {
        const int n = snprintf(out + used, out_size - used, "%s0x%08X/%uKB", i ? " " : "",
                               (unsigned)s_blocks[i].base, (unsigned)(s_blocks[i].size / 1024));
        if (n < 0)
            break;
        used += (size_t)n;
    }
    if (s_failures > 0 && used < out_size)
        snprintf(out + used, out_size - used, "%sfailed calls %d, first 0x%08X, last 0x%08X for %u KB",
                 used ? ", " : "", s_failures, (unsigned)s_first_error, (unsigned)s_last_error,
                 (unsigned)(s_last_failed_size / 1024));
    used = strlen(out);
    if (s_domain_failures > 0 && used < out_size)
        snprintf(out + used, out_size - used, "%sfailed domain calls %d, first 0x%08X", used ? ", " : "",
                 s_domain_failures, (unsigned)s_domain_first_error);
}
