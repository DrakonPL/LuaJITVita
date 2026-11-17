#ifndef LJB_RUNNER_H
#define LJB_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct RunnerConfig {
    const char *lib;
    const char *heap; // the heap column of the CSV
    bool take_heap_block;
    const char *package_path;
    void *arena; // NULL: the system allocator. Else the nolock allocator on [arena, arena + arena_size).
    size_t arena_size;
} RunnerConfig;

// Reads jit.version and jit.status() from a new Lua state. Returns false on failure.
bool runner_read_jit_info(const char *package_path, char *version, size_t version_size, char *status,
                          size_t status_size);

// Runs each bench of scripts/benches.lua in the order interp, jit, interp, jit, interp, jit.
// Writes one CSV row for each bench and mode. Returns the count of benches with ERROR or MISMATCH.
// Then samples the profile bench of scripts/benches.lua and writes the result as "# profile" lines.
int runner_run_all(const RunnerConfig *config, FILE *csv);

#endif
