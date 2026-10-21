#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# RONDO's own host checks: the CYCLE engines and the voice layout (tests/rondo_test.c), with demo WAVs in
# build/rondo_demo. Run from the repo root after the generate step (build/gen):
#   sh tests/run_rondo.sh            (NOCOST=1 skips the timing comparison)
set -e
cd "$(dirname "$0")/.."
[ -f build/gen/felucca_tables.h ] || { echo "run the generate step first (RONDO.md, Building)"; exit 1; }
mkdir -p build/host build/rondo_demo
${CC:-cc} -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/rondo_test tests/rondo_test.c -lm
build/host/rondo_test build/rondo_demo
