#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# RONDO: the browser emulator (web/emu) built with a stock clang and the WASI sysroot instead of Emscripten.
#   tools/build_emu_clang.sh [OUT]          (default OUT: build/emu)
# Needs: clang with the wasm32 target and wasm-ld (any LLVM 15+), and wasi-sdk's sysroot and builtins
# (https://github.com/WebAssembly/wasi-sdk/releases: wasi-sysroot-*.tar.gz, libclang_rt.builtins-wasm32-wasi-*.tar.gz):
#   WASI_SYSROOT   the unpacked wasi-sysroot (default: ~/wasi/wasi-sysroot-25.0)
#   WASI_RT        the unpacked builtins folder (default: ~/wasi/libclang_rt.builtins-wasm32-wasi-25.0)
# Run tools/build.py's generate step once first (build/gen), e.g.
#   python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
# web/emu/build.sh (Emscripten) still works as upstream; the page and worklet take either build.
set -e
cd "$(dirname "$0")/.."
OUT=${1:-build/emu}
SYSROOT=${WASI_SYSROOT:-$HOME/wasi/wasi-sysroot-25.0}
RT=${WASI_RT:-$HOME/wasi/libclang_rt.builtins-wasm32-wasi-25.0}
[ -f build/gen/felucca_tables.h ] || { echo "emu: no build/gen (run the generate step first)"; exit 1; }
[ -d "$SYSROOT" ] || { echo "emu: no WASI sysroot at $SYSROOT"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c | head -n 1)
STUB=build/wasi_stub
mkdir -p "$OUT/fonts" "$STUB/sys"
# WASI has no processes: hostsim.c's fork / wait helpers (tests only) are never called in the emulator
cat > "$STUB/sys/wait.h" <<'EOF'
#pragma once
#include <sys/types.h>
static inline int fork(void) { return -1; }
static inline int waitpid(int p, int *s, int o) { (void)p; (void)o; if (s) *s = 0; return -1; }
static inline int pipe(int f[2]) { (void)f; return -1; }
#define WIFEXITED(s) 1
#define WEXITSTATUS(s) 0
EOF
clang --target=wasm32-wasi --sysroot="$SYSROOT" -O2 -ffp-contract=off -std=gnu99 -w \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -I"$STUB" -mexec-model=reactor \
    -Wl,--no-entry -Wl,--export-dynamic -Wl,--gc-sections -Wl,-z,stack-size=1048576 -Wl,--initial-memory=33554432 \
    -nodefaultlibs -o "$OUT/felucca.wasm" web/emu/felucca_web.c -lc "$RT/libclang_rt.builtins-wasm32.a"
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@//g" web/emu/index.html > "$OUT/index.html"
cp web/emu/worklet.js "$OUT/"
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt "$OUT/fonts/"
echo "emu: $OUT ($(wc -c < "$OUT/felucca.wasm" | tr -d ' ') B wasm, $VERSION)"
