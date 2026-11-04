#!/usr/bin/env bash
# Boot bootloader + application in QEMU (netduinoplus2, STM32F4 model) and decode the telemetry.
#   scripts/qemu_run.sh [seconds] [firmware-build-dir] [host-build-dir]
set -euo pipefail
secs="${1:-5}"
fw="${2:-build/firmware}/firmware/sn_full_qemu.bin"
decode="${3:-build/host}/tools/sn_decode"
out="$(mktemp)"
trap 'rm -f "$out"' EXIT
# serial0 = USART1 (unused), serial1 = USART2 (ST-LINK VCP on the Nucleo).
timeout --preserve-status "$secs" qemu-system-arm -M netduinoplus2 -display none -monitor none \
    -serial null -serial "file:$out" -kernel "$fw" || true
"$decode" --port "$out"
