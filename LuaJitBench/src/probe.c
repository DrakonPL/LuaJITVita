#include "probe.h"

#include "mcode_wrap.h"
#include "platform.h"

#include <psp2/kernel/sysmem.h>

static const uint32_t kArmMovR0Imm42 = 0xE3A0002A;
static const uint32_t kArmBxLr = 0xE12FFF1E;
static const int kExpectedValue = 42;

enum {
    kChurnBlockSize = 1024 * 1024,
    kChurnCount = 64, // 64 MB in all: more than the VM space if each free leaks
    kMaxDistinctBases = 8,
};

// In SceSysmem_stub, but not in the vitasdk headers.
int sceKernelFreeMemBlockForVM(SceUID uid);

typedef int (*ProbeFn)(void);

static ProbeResult probe_fail(ProbeResult result, SceUID uid, int step, int error)
{
    if (uid >= 0)
        sceKernelFreeMemBlockForVM(uid);
    result.ok = false;
    result.failed_step = step;
    result.error = error;
    plat_log("probe step %d FAILED: 0x%08X", step, (unsigned)error);
    return result;
}

ProbeResult probe_vm_block(uint32_t block_size)
{
    ProbeResult result = { 0 };
    result.block_size = block_size;

    plat_log("probe 1: alloc VM block of %u KB", (unsigned)(block_size / 1024));
    const SceUID uid = sceKernelAllocMemBlockForVM("ljb_probe", block_size);
    if (uid < 0)
        return probe_fail(result, -1, 1, uid);
    void *base = NULL;
    int error = sceKernelGetMemBlockBase(uid, &base);
    if (error < 0)
        return probe_fail(result, uid, 1, error);
    result.block_base = (uintptr_t)base;
    result.distance_mb = vm_block_distance_mb(result.block_base, block_size);

    plat_log("probe 2: open VM domain");
    error = sceKernelOpenVMDomain();
    if (error < 0)
        return probe_fail(result, uid, 2, error);

    plat_log("probe 3: write 2 ARM instructions at 0x%08X", (unsigned)result.block_base);
    // volatile: the compiler must not drop stores to memory that it never sees read.
    volatile uint32_t *code = (volatile uint32_t *)base;
    code[0] = kArmMovR0Imm42;
    code[1] = kArmBxLr;

    plat_log("probe 4: sync and close VM domain");
    error = sceKernelSyncVMDomain(uid, base, 2 * sizeof(uint32_t));
    if (error < 0) {
        sceKernelCloseVMDomain();
        return probe_fail(result, uid, 4, error);
    }
    error = sceKernelCloseVMDomain();
    if (error < 0)
        return probe_fail(result, uid, 4, error);

    plat_log("probe 5: call 0x%08X (a crash here = no exec)", (unsigned)result.block_base);
    // volatile: the call must go through a register. The address is even, so BLX enters ARM state.
    ProbeFn volatile call = (ProbeFn)(uintptr_t)base;
    const int value = call();
    if (value != kExpectedValue)
        return probe_fail(result, uid, 5, value);

    plat_log("probe 6: result %d, block 0x%08X, %.1f MB to lj_vm_exit_handler 0x%08X", value,
             (unsigned)result.block_base, result.distance_mb, (unsigned)vm_exit_handler_address());

    plat_log("probe 7: free VM block (ForVM)");
    error = sceKernelFreeMemBlockForVM(uid);
    if (error < 0)
        return probe_fail(result, -1, 7, error);

    result.ok = true;
    return result;
}

void probe_describe(const ProbeResult *result, const char *label, char *out, size_t out_size)
{
    if (result->ok) {
        snprintf(out, out_size, "probe %s %u KB: OK, block 0x%08X, %.1f MB to lj_vm_exit_handler", label,
                 (unsigned)(result->block_size / 1024), (unsigned)result->block_base, result->distance_mb);
        return;
    }
    snprintf(out, out_size, "probe %s %u KB: FAIL at step %d, error 0x%08X", label,
             (unsigned)(result->block_size / 1024), result->failed_step, (unsigned)result->error);
}

static int count_distinct(const uintptr_t *bases, int count, uintptr_t base)
{
    const int kept = count < kMaxDistinctBases ? count : kMaxDistinctBases;
    for (int i = 0; i < kept; i++)
        if (bases[i] == base)
            return count;
    return count + 1;
}

ChurnResult probe_vm_free_churn(bool for_vm)
{
    ChurnResult result = { 0 };
    uintptr_t bases[kMaxDistinctBases];
    const char *free_name = for_vm ? "sceKernelFreeMemBlockForVM" : "sceKernelFreeMemBlock";
    plat_log("churn: %d x alloc 1 MB VM block, then %s", kChurnCount, free_name);

    for (int i = 0; i < kChurnCount; i++) {
        const SceUID uid = sceKernelAllocMemBlockForVM("ljb_churn", kChurnBlockSize);
        if (uid < 0) {
            result.failed_step = 1;
            result.error = uid;
            break;
        }
        void *base = NULL;
        sceKernelGetMemBlockBase(uid, &base);
        const int distinct = count_distinct(bases, result.distinct_bases, (uintptr_t)base);
        if (distinct > result.distinct_bases && distinct <= kMaxDistinctBases)
            bases[distinct - 1] = (uintptr_t)base;
        result.distinct_bases = distinct;

        const int error = for_vm ? sceKernelFreeMemBlockForVM(uid) : sceKernelFreeMemBlock(uid);
        if (error < 0) {
            result.failed_step = 2;
            result.error = error;
            break;
        }
        result.done++;
    }
    return result;
}

void churn_describe(const ChurnResult *result, bool for_vm, char *out, size_t out_size)
{
    const char *free_name = for_vm ? "sceKernelFreeMemBlockForVM" : "sceKernelFreeMemBlock";
    if (result->failed_step == 0) {
        snprintf(out, out_size, "churn %s: OK, %d of %d, %d distinct addresses", free_name, result->done,
                 kChurnCount, result->distinct_bases);
        return;
    }
    snprintf(out, out_size, "churn %s: FAIL at pair %d (%s), error 0x%08X, %d distinct addresses", free_name,
             result->done + 1, result->failed_step == 1 ? "alloc" : "free", (unsigned)result->error,
             result->distinct_bases);
}

void probe_vm_domain_twice(char *out, size_t out_size)
{
    plat_log("domain test: open, open, close, close");
    const int open1 = sceKernelOpenVMDomain();
    const int open2 = sceKernelOpenVMDomain();
    const int close1 = sceKernelCloseVMDomain();
    const int close2 = sceKernelCloseVMDomain();
    snprintf(out, out_size, "domain test: open 0x%08X, open 0x%08X, close 0x%08X, close 0x%08X", (unsigned)open1,
             (unsigned)open2, (unsigned)close1, (unsigned)close2);
}
