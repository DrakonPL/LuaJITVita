#!/usr/bin/env bash
# Builds the Docker image luajitbench-vita.
set -euo pipefail
cd "$(dirname "$0")/.."
echo "== build image $(date +%H:%M:%S)"
docker build -t luajitbench-vita docker/
echo "== done $(date +%H:%M:%S)"
