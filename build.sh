#!/usr/bin/env bash
# Build release and run unit tests. Usage: ./build.sh
set -e
cd "$(dirname "$0")"
cmake --preset release > build/configure.log 2>&1 || { tail -30 build/configure.log; exit 1; }
cmake --build --preset release > build/build.log 2>&1 || { grep -E "error|Error" build/build.log | head -40; exit 1; }
grep -E "warning C" build/build.log | sort -u | head -20 || true
ctest --preset release --output-on-failure 2>&1 | tail -25
