#include "runner.h"

#include "arena_alloc.h"
#include "bench_api.h"
#include "platform.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

enum {
    kRepeats = 3,
    kMaxBenches = 32,
    kNameMax = 48,
    kTextMax = 160,
    kChecksumMax = 64,
    kBlocksTextMax = 1024,
    kProfileSeconds = 2,
};

typedef enum Mode { MODE_INTERP, MODE_JIT, MODE_COUNT } Mode;

static const char *const kModeNames[MODE_COUNT] = { "interp", "jit" };

typedef struct RunResult {
    bool ok;
    char error[kTextMax];
    double ms;
    char checksum[kChecksumMax]; // a number as %.17g, or the text that run(n) returned
    int starts;
    int traces; // stopped traces
    int aborts;
    char top_abort[kTextMax];
    double mcode_kb;
    VmBlockStats vm;
    char vm_blocks[kBlocksTextMax];
} RunResult;

typedef struct BenchList {
    int count;
    char names[kMaxBenches][kNameMax];
    char profile[kNameMax]; // empty: no profile run
} BenchList;

// arena NULL: the system allocator (luaL_newstate).
static lua_State *new_state_on(const char *package_path, void *arena, size_t arena_size)
{
    lua_State *L = arena ? arena_newstate(arena, arena_size) : luaL_newstate();
    if (!L)
        return NULL;
    luaL_openlibs(L);
    lua_getglobal(L, "package");
    lua_pushstring(L, package_path);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
    bench_api_register(L);
    return L;
}

static lua_State *new_state(const char *package_path)
{
    return new_state_on(package_path, NULL, 0);
}

static void copy_text(char *out, size_t out_size, const char *text)
{
    snprintf(out, out_size, "%s", text ? text : "(no text)");
}

bool runner_read_jit_info(const char *package_path, char *version, size_t version_size, char *status,
                          size_t status_size)
{
    static const char *const kScript =
        "local jit = require('jit')\n"
        "local parts = { jit.status() }\n"
        "for i = 1, #parts do parts[i] = tostring(parts[i]) end\n"
        "return jit.version, table.concat(parts, ' ')\n";

    lua_State *L = new_state(package_path);
    if (!L)
        return false;
    const bool ok = luaL_dostring(L, kScript) == 0;
    if (ok) {
        copy_text(version, version_size, lua_tostring(L, -2));
        copy_text(status, status_size, lua_tostring(L, -1));
    } else {
        plat_log("jit info: %s", lua_tostring(L, -1));
    }
    lua_close(L);
    return ok;
}

static bool read_bench_list(const char *package_path, BenchList *list)
{
    list->count = 0;
    lua_State *L = new_state(package_path);
    if (!L)
        return false;
    if (luaL_dostring(L, "return require('benches')") != 0 || !lua_istable(L, -1)) {
        plat_log("bench list: %s", lua_isstring(L, -1) ? lua_tostring(L, -1) : "not a table");
        lua_close(L);
        return false;
    }
    const int size = (int)lua_objlen(L, -1);
    if (size > kMaxBenches)
        plat_log("bench list: %d benches, only the first %d run", size, kMaxBenches);
    for (int i = 1; i <= size && list->count < kMaxBenches; i++) {
        lua_rawgeti(L, -1, i);
        copy_text(list->names[list->count++], kNameMax, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_getfield(L, -1, "profile");
    list->profile[0] = '\0';
    if (lua_isstring(L, -1))
        copy_text(list->profile, kNameMax, lua_tostring(L, -1));
    lua_close(L);
    return true;
}

static bool call_lua(lua_State *L, int nargs, int nresults, RunResult *out)
{
    if (lua_pcall(L, nargs, nresults, 0) == 0)
        return true;
    copy_text(out->error, sizeof out->error, lua_tostring(L, -1));
    return false;
}

// Stack before: harness. Stack after: harness.
static bool run_bench_in_state(lua_State *L, const char *bench, Mode mode, RunResult *out)
{
    const int harness = lua_gettop(L);
    lua_getfield(L, harness, "prepare");
    lua_pushstring(L, bench);
    lua_pushstring(L, kModeNames[mode]);
    if (!call_lua(L, 2, 2, out))
        return false;

    const uint64_t start_us = plat_time_us();
    const bool ran = call_lua(L, 1, 1, out);
    const uint64_t end_us = plat_time_us();
    if (!ran)
        return false;
    out->ms = (double)(end_us - start_us) / 1000.0;
    if (lua_type(L, -1) == LUA_TNUMBER) {
        snprintf(out->checksum, sizeof out->checksum, "%.17g", lua_tonumber(L, -1));
    } else if (lua_type(L, -1) == LUA_TSTRING) {
        copy_text(out->checksum, sizeof out->checksum, lua_tostring(L, -1));
    } else {
        copy_text(out->error, sizeof out->error, "run(n) returned no number and no string");
        return false;
    }
    lua_settop(L, harness);

    lua_getfield(L, harness, "finish");
    if (!call_lua(L, 0, 5, out))
        return false;
    out->starts = (int)lua_tointeger(L, -5);
    out->traces = (int)lua_tointeger(L, -4);
    out->aborts = (int)lua_tointeger(L, -3);
    copy_text(out->top_abort, sizeof out->top_abort, lua_tostring(L, -2));
    out->mcode_kb = (double)lua_tointeger(L, -1) / 1024.0;
    lua_settop(L, harness);
    return true;
}

static RunResult run_once(const RunnerConfig *config, const char *bench, Mode mode)
{
    RunResult result;
    memset(&result, 0, sizeof result);

    if (config->take_heap_block && !plat_heap_block_take()) {
        copy_text(result.error, sizeof result.error, "heap block alloc failed");
        return result;
    }
    plat_vm_blocks_reset();

    lua_State *L = new_state_on(config->package_path, config->arena, config->arena_size);
    if (!L) {
        copy_text(result.error, sizeof result.error, "new Lua state failed");
    } else {
        if (luaL_dostring(L, "return require('harness')") != 0)
            copy_text(result.error, sizeof result.error, lua_tostring(L, -1));
        else
            result.ok = run_bench_in_state(L, bench, mode, &result);
        lua_close(L);
    }

    result.vm = plat_vm_blocks_stats();
    plat_vm_blocks_describe(result.vm_blocks, sizeof result.vm_blocks);
    if (config->take_heap_block)
        plat_heap_block_free();
    return result;
}

static int compare_double(const void *a, const void *b)
{
    const double x = *(const double *)a;
    const double y = *(const double *)b;
    return (x > y) - (x < y);
}

static void write_csv_text(FILE *csv, const char *text)
{
    if (!strpbrk(text, ",\"\n")) {
        fputs(text, csv);
        return;
    }
    fputc('"', csv);
    for (const char *c = text; *c; c++) {
        if (*c == '"')
            fputc('"', csv);
        fputc(*c, csv);
    }
    fputc('"', csv);
}

typedef struct ModeSummary {
    double median_ms;
    double min_ms;
    double max_ms;
    bool error;
    bool mismatch;
    const char *checksum; // the first wrong value on a mismatch
    const RunResult *last;
} ModeSummary;

// The checksum of each run must equal the checksum of the first run of the bench.
static ModeSummary summarize(const RunResult runs[kRepeats], const char *reference)
{
    ModeSummary summary;
    memset(&summary, 0, sizeof summary);
    summary.checksum = reference;
    summary.last = &runs[kRepeats - 1];

    double times[kRepeats];
    int timed = 0;
    for (int i = 0; i < kRepeats; i++) {
        if (!runs[i].ok) {
            summary.error = true;
            continue;
        }
        times[timed++] = runs[i].ms;
        if (strcmp(runs[i].checksum, reference) != 0 && !summary.mismatch) {
            summary.mismatch = true;
            summary.checksum = runs[i].checksum;
        }
    }
    if (timed == 0)
        return summary;
    qsort(times, (size_t)timed, sizeof times[0], compare_double);
    summary.median_ms = times[timed / 2];
    summary.min_ms = times[0];
    summary.max_ms = times[timed - 1];
    return summary;
}

static void write_row(FILE *csv, const RunnerConfig *config, const char *bench, Mode mode, const ModeSummary *s)
{
    fprintf(csv, "%s,%s,%s,%s,%.3f,%.3f,%.3f,", config->lib, config->heap, bench, kModeNames[mode], s->median_ms,
            s->min_ms, s->max_ms);
    if (s->error)
        fputs("ERROR", csv);
    else {
        char checksum[kChecksumMax + 16];
        snprintf(checksum, sizeof checksum, "%s%s", s->mismatch ? "MISMATCH:" : "", s->checksum);
        write_csv_text(csv, checksum);
    }
    fprintf(csv, ",%d,%d,", s->last->traces, s->last->aborts);
    write_csv_text(csv, s->last->top_abort);
    fprintf(csv, ",%.1f,", s->last->mcode_kb);
    if (s->last->vm.known)
        fprintf(csv, "%d,%.1f\n", s->last->vm.count, s->last->vm.max_distance_mb);
    else
        fputs("na,na\n", csv);
    fprintf(csv, "# run %s %s: starts %d, vm blocks: %s\n", bench, kModeNames[mode], s->last->starts,
            s->last->vm_blocks);
    if (s->last->error[0])
        fprintf(csv, "# error %s %s: %s\n", bench, kModeNames[mode], s->last->error);
    fflush(csv);
}

static const RunResult *first_ok_run(const RunResult runs[MODE_COUNT][kRepeats])
{
    // The order of the runs: interp 1, jit 1, interp 2, jit 2, interp 3, jit 3.
    for (int rep = 0; rep < kRepeats; rep++)
        for (int mode = 0; mode < MODE_COUNT; mode++)
            if (runs[mode][rep].ok)
                return &runs[mode][rep];
    return NULL;
}

static bool run_bench(const RunnerConfig *config, const char *bench, FILE *csv, RunResult runs[MODE_COUNT][kRepeats])
{
    for (int rep = 0; rep < kRepeats; rep++) {
        for (int mode = 0; mode < MODE_COUNT; mode++) {
            plat_log("%s %s %d/%d", bench, kModeNames[mode], rep + 1, kRepeats);
            // Flushed before the run: after a crash, the last such line names the run.
            fprintf(csv, "# start %s %s %d/%d\n", bench, kModeNames[mode], rep + 1, kRepeats);
            fflush(csv);
            runs[mode][rep] = run_once(config, bench, (Mode)mode);
            if (!runs[mode][rep].ok)
                plat_log("  ERROR: %s", runs[mode][rep].error);
        }
    }

    const RunResult *first = first_ok_run((const RunResult(*)[kRepeats])runs);
    const char *reference = first ? first->checksum : "";
    ModeSummary summaries[MODE_COUNT];
    bool clean = true;
    for (int mode = 0; mode < MODE_COUNT; mode++) {
        summaries[mode] = summarize(runs[mode], reference);
        write_row(csv, config, bench, (Mode)mode, &summaries[mode]);
        clean = clean && !summaries[mode].error && !summaries[mode].mismatch;
    }

    const ModeSummary *interp = &summaries[MODE_INTERP];
    const ModeSummary *jit = &summaries[MODE_JIT];
    const double gain = jit->median_ms > 0.0 ? interp->median_ms / jit->median_ms : 0.0;
    plat_log("%-12s interp %8.1f ms  jit %8.1f ms  gain %5.2fx  traces %d  aborts %d%s", bench, interp->median_ms,
             jit->median_ms, gain, jit->last->traces, jit->last->aborts, clean ? "" : "  ERROR/MISMATCH");
    if (jit->last->top_abort[0])
        plat_log("  top abort: %s", jit->last->top_abort);
    if (jit->last->vm.failures > 0)
        plat_log("  VM block calls failed: %d, last error 0x%08X", jit->last->vm.failures,
                 (unsigned)jit->last->vm.last_error);
    if (jit->last->vm.domain_failures > 0)
        plat_log("  VM domain calls failed: %d, first error 0x%08X", jit->last->vm.domain_failures,
                 (unsigned)jit->last->vm.domain_first_error);
    return clean;
}

// Writes each line of text as a "# " line of the CSV file and as a screen line.
static void write_comment_lines(FILE *csv, const char *text)
{
    while (*text) {
        const char *end = strchr(text, '\n');
        const int length = end ? (int)(end - text) : (int)strlen(text);
        fprintf(csv, "# %.*s\n", length, text);
        plat_log("%.*s", length, text);
        text += length + (end ? 1 : 0);
    }
    fflush(csv);
}

static void run_profile(const RunnerConfig *config, const char *bench, FILE *csv)
{
    plat_log("profile %s for %d s", bench, kProfileSeconds);
    fprintf(csv, "# start profile %s\n", bench);
    fflush(csv);
    lua_State *L = new_state_on(config->package_path, config->arena, config->arena_size);
    if (!L) {
        write_comment_lines(csv, "profile: new Lua state failed");
        return;
    }
    if (luaL_dostring(L, "return require('harness')") != 0) {
        write_comment_lines(csv, lua_tostring(L, -1));
        lua_close(L);
        return;
    }
    lua_getfield(L, -1, "profile");
    lua_pushstring(L, bench);
    lua_pushinteger(L, kProfileSeconds);
    if (lua_pcall(L, 2, 1, 0) != 0)
        fprintf(csv, "# profile error: ");
    write_comment_lines(csv, lua_tostring(L, -1) ? lua_tostring(L, -1) : "profile: no result");
    lua_close(L);
}

int runner_run_all(const RunnerConfig *config, FILE *csv)
{
    static BenchList list;
    // About 1.2 KB per run: static, so that the runs stay off the small main stack of the Vita.
    static RunResult runs[MODE_COUNT][kRepeats];

    if (!read_bench_list(config->package_path, &list))
        return 1;

    fputs("lib,heap,bench,mode,median_ms,min_ms,max_ms,checksum,traces,aborts,top_abort,mcode_kb,vm_blocks,"
          "max_distance_mb\n",
          csv);
    fflush(csv);

    int problems = 0;
    for (int i = 0; i < list.count; i++)
        if (!run_bench(config, list.names[i], csv, runs))
            problems++;
    if (list.profile[0])
        run_profile(config, list.profile, csv);
    return problems;
}
