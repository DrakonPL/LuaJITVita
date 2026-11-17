#ifndef LJB_PROBE_H
#define LJB_PROBE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ProbeResult {
    bool ok;
    int failed_step; // 0 when ok
    int error;       // the SCE error of the failed step, or the wrong result of step 5
    uint32_t block_size; // bytes
    uintptr_t block_base;
    double distance_mb; // from the far end of the block to lj_vm_exit_handler
} ProbeResult;

// Writes "mov r0, #42; bx lr" into a new VM block of block_size bytes and calls it. Uses no LuaJIT code.
ProbeResult probe_vm_block(uint32_t block_size);

void probe_describe(const ProbeResult *result, const char *label, char *out, size_t out_size);

typedef struct ChurnResult {
    int done;           // alloc and free pairs that worked
    int failed_step;    // 0 when all worked, 1 = alloc, 2 = free
    int error;
    int distinct_bases; // 1 when the kernel gave the same address each time
} ChurnResult;

// Allocates and frees a 1 MB VM block many times. A free that does not release the block fails the alloc
// before the end. for_vm: free with sceKernelFreeMemBlockForVM in place of sceKernelFreeMemBlock.
ChurnResult probe_vm_free_churn(bool for_vm);

// Opens the VM domain two times, then closes it two times. Writes the four results.
void probe_vm_domain_twice(char *out, size_t out_size);
void churn_describe(const ChurnResult *result, bool for_vm, char *out, size_t out_size);

#endif
