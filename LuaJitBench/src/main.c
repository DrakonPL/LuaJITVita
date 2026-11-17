#include "platform.h"
#include "runner.h"

#ifdef __vita__
#include "probe.h"
#endif

#include <stdarg.h>
#include <string.h>

#ifndef LJB_LIB
#error "the Makefile must define LJB_LIB, the name of the LuaJIT lib"
#endif

// "safe" or "unsafe" on the Vita: the kind of eboot.bin that vita-make-fself made.
#ifndef LJB_FSELF
#define LJB_FSELF "none"
#endif

// The build flag ALLOC=nolock: each Lua state gets a TLSF pool with no lock in place of newlib malloc.
#ifdef LJB_ALLOC_NOLOCK
static const char *const kAllocName = "nolock";
#else
static const char *const kAllocName = "sys";
#endif

enum {
    kMaxHeaderLines = 12,
    kHeaderLineMax = 256,
    kHeapNameMax = 32,
    kBaseTagMax = 64,
    kTagMax = 96, // the base tag plus _nolock and _unsafe
    kPathMax = 256,
    kMaxFileNumber = 99,
};

static const size_t kArenaSize = 32 * 1024 * 1024;

typedef struct Header {
    int count;
    char lines[kMaxHeaderLines][kHeaderLineMax];
} Header;

static void header_add(Header *header, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void header_add(Header *header, const char *fmt, ...)
{
    if (header->count >= kMaxHeaderLines)
        return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(header->lines[header->count++], kHeaderLineMax, fmt, args);
    va_end(args);
}

// "small" and "block" in the default build, "bigheap" and "bigheap_block" in the BIG_HEAP build.
static void heap_name(bool with_block, char *out, size_t out_size)
{
    const char *base = plat_base_heap_name();
    if (!with_block)
        snprintf(out, out_size, "%s", base);
    else if (strcmp(base, "small") == 0)
        snprintf(out, out_size, "block");
    else
        snprintf(out, out_size, "%s_block", base);
}

#ifdef __vita__
// One report gives a floor of 1 MB for a VM block. The probe tries both sides of it.
static const uint32_t kProbeBlockSizes[] = { 64 * 1024, 1024 * 1024 };
enum { kProbeSizeCount = sizeof kProbeBlockSizes / sizeof kProbeBlockSizes[0] };

static void add_probes(Header *header, const char *label)
{
    char line[kHeaderLineMax];
    for (int i = 0; i < kProbeSizeCount; i++) {
        const ProbeResult result = probe_vm_block(kProbeBlockSizes[i]);
        probe_describe(&result, label, line, sizeof line);
        header_add(header, "%s", line);
    }
}

static void add_probe_lines(Header *header)
{
    char label[kHeapNameMax];

    char line[kHeaderLineMax];
    probe_vm_domain_twice(line, sizeof line);
    header_add(header, "%s", line);

    heap_name(false, label, sizeof label);
    add_probes(header, label);

    heap_name(true, label, sizeof label);
    if (!plat_heap_block_take()) {
        header_add(header, "probe %s: SKIPPED, the heap block alloc failed", label);
        return;
    }
    add_probes(header, label);
    plat_heap_block_free();
}
#else
static void add_probe_lines(Header *header)
{
    header_add(header, "probe: none on the host");
}
#endif

static void build_header(Header *header)
{
    header->count = 0;
    add_probe_lines(header);

    char version[64];
    char status[192];
    if (!runner_read_jit_info(plat_package_path(), version, sizeof version, status, sizeof status))
        snprintf(version, sizeof version, "unknown");
    header_add(header, "luajit: %s, jit.status: %s", version, status);
    header_add(header, "cpu: %d MHz", plat_cpu_mhz());
    header_add(header, "lib: %s", LJB_LIB);
    header_add(header, "fself: %s", LJB_FSELF);
    header_add(header, "alloc: %s", kAllocName);
}

// Opens results_<tag>.csv, or results_<tag>_2.csv and so on if the file exists. A new run never
// writes over an old file. Returns NULL on failure.
static FILE *open_new_result_file(const char *tag)
{
    const char *dir = plat_results_dir();
    char path[kPathMax];
    for (int number = 1; number <= kMaxFileNumber; number++) {
        if (number == 1)
            snprintf(path, sizeof path, "%s/results_%s.csv", dir, tag);
        else
            snprintf(path, sizeof path, "%s/results_%s_%d.csv", dir, tag, number);
        FILE *existing = fopen(path, "r");
        if (existing) {
            fclose(existing);
            continue;
        }
        plat_log("file: %s", path);
        return fopen(path, "w");
    }
    return NULL;
}

// <lib>_<heap>, plus _nolock and _unsafe, so that each build keeps its own files.
static void result_tag(const char *base, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s%s%s", base, strcmp(kAllocName, "sys") != 0 ? "_nolock" : "",
             strcmp(LJB_FSELF, "unsafe") == 0 ? "_unsafe" : "");
}

static int run_all(const Header *header, bool with_block)
{
    char heap[kHeapNameMax];
    heap_name(with_block, heap, sizeof heap);

    plat_clear_screen();
    for (int i = 0; i < header->count; i++)
        plat_log("%s", header->lines[i]);
    plat_log("heap: %s", heap);

    char base[kBaseTagMax];
    snprintf(base, sizeof base, "%s_%s", LJB_LIB, heap);
    char tag[kTagMax];
    result_tag(base, tag, sizeof tag);
    FILE *csv = open_new_result_file(tag);
    if (!csv) {
        plat_log("ERROR: cannot open the CSV file");
        return 1;
    }
    for (int i = 0; i < header->count; i++)
        fprintf(csv, "# %s\n", header->lines[i]);
    fprintf(csv, "# heap: %s\n", heap);
    fflush(csv);

    RunnerConfig config = { LJB_LIB, heap, with_block, plat_package_path(), NULL, 0 };
    if (strcmp(kAllocName, "nolock") == 0) {
        config.arena = plat_arena(kArenaSize);
        config.arena_size = kArenaSize;
        if (!config.arena) {
            fclose(csv);
            plat_log("ERROR: no memory for the nolock allocator");
            return 1;
        }
    }
    const int problems = runner_run_all(&config, csv);
    fclose(csv);
    plat_log("done: %d benches with ERROR or MISMATCH", problems);
    return problems;
}

#ifdef __vita__
// A free that leaks fills the VM space for the rest of the process. So run one test for each start of the app.
static void run_free_test(const Header *header, bool for_vm)
{
    plat_clear_screen();
    const ChurnResult result = probe_vm_free_churn(for_vm);
    char line[kHeaderLineMax];
    churn_describe(&result, for_vm, line, sizeof line);
    plat_log("%s", line);

    char tag[kTagMax];
    snprintf(tag, sizeof tag, "%s_free_test%s", LJB_LIB, for_vm ? "_for_vm" : "");
    FILE *file = open_new_result_file(tag);
    if (!file) {
        plat_log("ERROR: cannot open the result file");
        return;
    }
    for (int i = 0; i < header->count; i++)
        fprintf(file, "# %s\n", header->lines[i]);
    fprintf(file, "# %s\n", line);
    fclose(file);
}
#else
static void run_free_test(const Header *header, bool for_vm)
{
    (void)header;
    (void)for_vm;
}
#endif

static void show_menu(bool with_block)
{
    char heap[kHeapNameMax];
    heap_name(with_block, heap, sizeof heap);
    plat_log("%s", "");
    plat_log("CROSS: run all   SQUARE: heap block on/off   START: quit   (heap: %s)", heap);
    plat_log("TRIANGLE: VM free test   CIRCLE: VM free test ForVM   (one test for each start)");
}

int main(int argc, char **argv)
{
    plat_init(argc, argv);

    static Header header;
    build_header(&header);
    for (int i = 0; i < header.count; i++)
        plat_log("%s", header.lines[i]);

    bool with_block = false;
    int problems = 0;
    for (bool running = true; running;) {
        show_menu(with_block);
        switch (plat_next_action()) {
        case PLAT_ACTION_RUN:
            problems = run_all(&header, with_block);
            break;
        case PLAT_ACTION_TOGGLE_HEAP_BLOCK:
            with_block = plat_has_heap_block() && !with_block;
            break;
        case PLAT_ACTION_FREE_TEST:
            run_free_test(&header, false);
            break;
        case PLAT_ACTION_FREE_FOR_VM_TEST:
            run_free_test(&header, true);
            break;
        case PLAT_ACTION_QUIT:
            running = false;
            break;
        }
    }

    plat_shutdown();
    return problems == 0 ? 0 : 1;
}
