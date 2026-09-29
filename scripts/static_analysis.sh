#!/usr/bin/env bash
# Static analysis gate: clang-tidy + cppcheck (fail on findings) and a MISRA C:2012 addon report.
#   Requires: configured `host` (clang) and `firmware` presets for compile databases and CMSIS/FreeRTOS headers.
#   scripts/static_analysis.sh [host-build-dir] [firmware-build-dir] [report-dir]
set -euo pipefail
host="${1:-build/host}"
fw="${2:-build/firmware}"
report="${3:-build/static-analysis}"
deps="$fw/_deps"
mkdir -p "$report/cppcheck-cache"
root="$(pwd)"

echo "== clang-tidy =="
run-clang-tidy -quiet -p "$host" "$root/(app|common|tools|sim)/.*\.(c|cpp)$"

echo "== cppcheck (host sources) =="
cppcheck --project="$host/compile_commands.json" -i "$host/_deps" -i tests \
    --enable=warning,style,performance,portability --inline-suppr \
    --suppress=missingIncludeSystem --suppress='*:*/_deps/*' \
    --error-exitcode=1 -q --cppcheck-build-dir="$report/cppcheck-cache"

fw_args=(--std=c11 -DSTM32F401xE -D__GNUC__ -D__ARM_ARCH_7EM__ -DSN_SLOT_ADDR=0x08020000U
    -DSN_VERSION_MAJOR=1U -DSN_VERSION_MINOR=0U -DSN_VERSION_PATCH=0U -DSN_GIT_HASH=0U
    -Icommon/include -Ifirmware/drivers/include -Ifirmware/board -Iapp/rtos -Ifirmware/config
    -I"$deps/cmsis_core-src/Core/Include" -I"$deps/cmsis_device_f4-src/Include"
    -I"$deps/freertos_kernel-src/include" -I"$deps/freertos_kernel-src/portable/GCC/ARM_CM4F"
    --suppress=missingIncludeSystem --suppress='*:*/_deps/*' --inline-suppr)

echo "== cppcheck (firmware) =="
cppcheck -q --enable=warning,style,performance,portability --error-exitcode=1 "${fw_args[@]}" firmware

echo "== MISRA C:2012 addon (report only) =="
cppcheck -q --addon=misra --enable=style "${fw_args[@]}" common/src firmware 2> "$report/misra.txt" || true
grep -oE 'misra-c2012-[0-9.]+' "$report/misra.txt" | sort | uniq -c | sort -rn | tee "$report/misra-summary.txt"
echo "MISRA report: $report/misra.txt"
