#ifndef LJB_MCODE_WRAP_H
#define LJB_MCODE_WRAP_H

#include <stdint.h>

// Distance in MB from lj_vm_exit_handler to the far end of the block [base, base + size).
double vm_block_distance_mb(uintptr_t base, uint32_t size);
uintptr_t vm_exit_handler_address(void);

#endif
