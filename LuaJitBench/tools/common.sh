# Shared settings for the tools. Source this file, do not run it.
BENCH_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# The LuaJIT repo. LuaJitBench is a folder of it.
FORK_DIR=${FORK_DIR:-$(cd "$BENCH_DIR/.." && pwd)}
IMAGE=luajitbench-vita

# The package recipe (vitasdk/packages luajit/VITABUILD) uses these flags.
VITA_MAKE_FLAGS="HOST_CC='gcc -m32' CROSS='arm-vita-eabi-' TARGET_SYS='PSP2' TARGET_FLAGS='-marm -fno-optimize-sibling-calls'"

host_path() {
    if command -v cygpath > /dev/null; then cygpath -w "$1"; else echo "$1"; fi
}

# Runs a bash command in the image, with the bench repo at /bench.
# MSYS_NO_PATHCONV stops Git Bash from changing the /bench path.
in_docker() {
    MSYS_NO_PATHCONV=1 docker run --rm -v "$(host_path "$BENCH_DIR"):/bench" -w /bench "$IMAGE" bash -c "$1"
}

step() {
    echo "== $* $(date +%H:%M:%S)"
}
