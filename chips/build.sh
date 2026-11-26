#!/usr/bin/env bash
# Builds the three Wokwi custom chips to WebAssembly and copies them, with their JSON
# description, to firmware/chips/ where firmware/wokwi.toml expects them.
# Requires clang with the wasm32 target and wasm-ld (e.g. `apt install clang lld`).
# Same flags as the official Wokwi inverter-chip example, minus the wasi-libc sysroot:
# the chips use no C library.
# Use the bare WebAssembly target: no WASI runtime or deprecated WASI alias.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$HERE/../firmware/chips"
mkdir -p "$OUT"

for chip in ir-break-beam piezo-sensor fsr-sensor; do
  echo "Building $chip"
  clang --target=wasm32-unknown-unknown -ffreestanding -nostdlib \
    -Wl,--import-memory -Wl,--export-table -Wl,--no-entry -Werror -O2 \
    -I "$HERE" -o "$OUT/$chip.chip.wasm" "$HERE/$chip/main.c"
  cp "$HERE/$chip/$chip.chip.json" "$OUT/$chip.chip.json"
done
echo "Done: $(ls "$OUT")"
