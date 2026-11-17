#!/usr/bin/env bash
# Runs make in the image, in the bench repo.
# Usage: tools/make.sh -f Makefile.vita LIB=rebase [BIG_HEAP=1]
#        tools/make.sh -f Makefile.host run
set -euo pipefail
source "$(dirname "$0")/common.sh"

cd "$BENCH_DIR"
in_docker "make $(printf '%q ' "$@")"
