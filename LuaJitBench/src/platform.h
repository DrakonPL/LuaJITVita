#ifndef LJB_PLATFORM_H
#define LJB_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum PlatAction {
    PLAT_ACTION_RUN,
    PLAT_ACTION_TOGGLE_HEAP_BLOCK,
    PLAT_ACTION_FREE_TEST,        // the VM block free test with sceKernelFreeMemBlock
    PLAT_ACTION_FREE_FOR_VM_TEST, // the same test with sceKernelFreeMemBlockForVM
    PLAT_ACTION_QUIT,
} PlatAction;

typedef struct VmBlockStats {
    int count;              // VM blocks allocated since the last reset
    int failures;           // failed VM block calls since the last reset
    int first_error;        // the SCE error of the first failed call
    int last_error;         // the SCE error of the last failed call
    int domain_failures;    // failed sceKernelOpenVMDomain and sceKernelCloseVMDomain calls
    int domain_first_error;
    double max_distance_mb; // largest distance of a block end to lj_vm_exit_handler
    bool known;             // false on the host: no VM blocks there
} VmBlockStats;

void plat_init(int argc, char **argv);
void plat_shutdown(void);

uint64_t plat_time_us(void);

// One line on the screen. The text has no line end.
void plat_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void plat_clear_screen(void);

PlatAction plat_next_action(void);

// The folder for the result files. The call makes the folder if it does not exist.
const char *plat_results_dir(void);

const char *plat_package_path(void);
int plat_cpu_mhz(void); // 0 when unknown

// The heap of the newlib before main: "small", "near", or "bigheap" on the Vita, "host" on the host.
const char *plat_base_heap_name(void);
bool plat_has_heap_block(void);
// Takes the 128 MB block that acts like the heap of a game. Returns false on failure.
bool plat_heap_block_take(void);
void plat_heap_block_free(void);

// One block of size bytes for the nolock allocator. The first call takes it, later calls return it.
// Returns NULL on failure.
void *plat_arena(size_t size);

void plat_vm_blocks_reset(void);
VmBlockStats plat_vm_blocks_stats(void);
// Writes the base and size of each kept VM block, then the failed calls.
void plat_vm_blocks_describe(char *out, size_t out_size);

#endif
