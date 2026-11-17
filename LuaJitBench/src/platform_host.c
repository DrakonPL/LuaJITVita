#include "platform.h"

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#ifndef LJB_HOST_PACKAGE_PATH
#error "Makefile.host must define LJB_HOST_PACKAGE_PATH"
#endif

static const char *s_results_dir = "results/host";
static int s_actions_done;

void plat_init(int argc, char **argv)
{
    if (argc > 1)
        s_results_dir = argv[1];
}

void plat_shutdown(void)
{
}

uint64_t plat_time_us(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000u + (uint64_t)now.tv_nsec / 1000u;
}

void plat_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
    fflush(stdout);
}

void plat_clear_screen(void)
{
}

// The host has no buttons: one run, then quit.
PlatAction plat_next_action(void)
{
    return s_actions_done++ == 0 ? PLAT_ACTION_RUN : PLAT_ACTION_QUIT;
}

const char *plat_results_dir(void)
{
    if (mkdir(s_results_dir, 0755) != 0 && errno != EEXIST)
        plat_log("ERROR: cannot make %s", s_results_dir);
    return s_results_dir;
}

const char *plat_package_path(void)
{
    return LJB_HOST_PACKAGE_PATH;
}

int plat_cpu_mhz(void)
{
    return 0;
}

const char *plat_base_heap_name(void)
{
    return "host";
}

bool plat_has_heap_block(void)
{
    return false;
}

bool plat_heap_block_take(void)
{
    return false;
}

void plat_heap_block_free(void)
{
}

void *plat_arena(size_t size)
{
    static void *arena;
    if (!arena)
        arena = malloc(size);
    return arena;
}

void plat_vm_blocks_reset(void)
{
}

VmBlockStats plat_vm_blocks_stats(void)
{
    return (VmBlockStats){ 0, 0, 0, 0, 0, 0, 0.0, false };
}

void plat_vm_blocks_describe(char *out, size_t out_size)
{
    snprintf(out, out_size, "none on the host");
}
