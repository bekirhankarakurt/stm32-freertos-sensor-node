# stm32-freertos-sensor-node

**Production-style STM32F401RE sensor node: register-level drivers, FreeRTOS pipeline, CRC-framed telemetry and a fail-safe A/B bootloader — fully testable on a PC.**

[![CI](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/actions/workflows/ci.yml/badge.svg)](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/actions/workflows/ci.yml)
[![CodeQL](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/actions/workflows/codeql.yml/badge.svg)](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/actions/workflows/codeql.yml)
[![Coverage](https://img.shields.io/endpoint?url=https%3A%2F%2Fbekirhankarakurt.github.io%2Fstm32-freertos-sensor-node%2Fcoverage%2Fbadge.json)](https://bekirhankarakurt.github.io/stm32-freertos-sensor-node/coverage/)
[![Release](https://img.shields.io/github/v/release/bekirhankarakurt/stm32-freertos-sensor-node)](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C11 / C++17](https://img.shields.io/badge/C11%20%2F%20C%2B%2B17-00599C?logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/platform-STM32F401RE%20%7C%20QEMU%20%7C%20Linux-03234B)

**[Documentation](https://bekirhankarakurt.github.io/stm32-freertos-sensor-node/)** — API reference (Doxygen) and coverage report · **[Releases](https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/releases)** — prebuilt `.elf/.bin/.hex` for bootloader and slots A/B

## Why / what

- **No HAL.** RCC, GPIO, USART2 (DMA TX + IRQ RX ring buffers), I2C1, SPI2, TIM2, IWDG and flash are driven at register level with bounded waits and fallbacks (HSE → HSI-PLL → HSI).
- **Deterministic RTOS design.** Five statically allocated FreeRTOS tasks (no heap, ever): fixed-rate sampling → median/EMA filtering + window statistics → telemetry, plus command handling and a task-aware watchdog.
- **Robust link.** Every message is `payload ‖ CRC-16` → COBS → `0x00`; one decoder serves telemetry, commands and firmware updates. Host decoder and uploader in C++17 share the exact same codec.
- **Fail-safe updates.** A/B slots with CRC-32 image headers, power-cut-tolerant boot journal, three trial boots and automatic rollback; updates over UART with retries and idempotent writes.
- **Testable without hardware.** The whole application runs on Linux (FreeRTOS POSIX port, simulated BME280) and the real firmware boots in QEMU; 58 unit tests plus end-to-end checks run under GCC, Clang and ASan/UBSan in CI.

## Demo

Real UART output of the **bootloader + application** booted in QEMU (`scripts/qemu_run.sh`), decoded by `sn_decode`:

```text
BOOT  v1.0.0 slot=A git=3b9219f8 reset=unknown trial_attempts=0 rolled_back=no
TELEM t=   0.100s  T= 23.53 C  P=  1013.23 hPa  RH= 44.94 %  Tmin=23.53 Tmax=23.53 Tsd=0.00  n=1  [ok]
TELEM t=   0.200s  T= 23.53 C  P=  1013.23 hPa  RH= 44.96 %  Tmin=23.53 Tmax=23.53 Tsd=0.00  n=2  [ok]
TELEM t=   0.300s  T= 23.53 C  P=  1013.23 hPa  RH= 44.96 %  Tmin=23.53 Tmax=23.53 Tsd=0.00  n=3  [ok]
TELEM t=   0.400s  T= 23.52 C  P=  1013.23 hPa  RH= 44.95 %  Tmin=23.52 Tmax=23.53 Tsd=0.00  n=4  [ok]
TELEM t=   0.500s  T= 23.52 C  P=  1013.23 hPa  RH= 44.94 %  Tmin=23.52 Tmax=23.53 Tsd=0.00  n=5  [ok]
TELEM t=   0.600s  T= 23.50 C  P=  1013.24 hPa  RH= 44.94 %  Tmin=23.50 Tmax=23.53 Tsd=0.01  n=6  [ok]
TELEM t=   0.700s  T= 23.49 C  P=  1013.24 hPa  RH= 44.95 %  Tmin=23.49 Tmax=23.53 Tsd=0.01  n=7  [ok]
TELEM t=   0.800s  T= 23.48 C  P=  1013.24 hPa  RH= 44.95 %  Tmin=23.48 Tmax=23.53 Tsd=0.01  n=8  [ok]
TELEM t=   0.900s  T= 23.47 C  P=  1013.24 hPa  RH= 44.95 %  Tmin=23.47 Tmax=23.53 Tsd=0.02  n=9  [ok]
TELEM t=   1.000s  T= 23.46 C  P=  1013.24 hPa  RH= 44.95 %  Tmin=23.46 Tmax=23.53 Tsd=0.02  n=10  [ok]
HEALTH t=  1.004s cpu=0.7% stack_min=186w reset=unknown rx_err=0 frame_err=0 tx_drop=0 sensor_err=0 wdt_late=0
...
TELEM t=   2.200s  T= 23.41 C  P=  1013.23 hPa  RH= 44.84 %  Tmin=23.41 Tmax=23.53 Tsd=0.04  n=22  [ok]
TELEM t=   2.300s  T= 23.41 C  P=  1013.23 hPa  RH= 44.84 %  Tmin=23.41 Tmax=23.53 Tsd=0.04  n=23  [ok]
TELEM t=   2.400s  T= 23.39 C  P=  1013.22 hPa  RH= 44.82 %  Tmin=23.39 Tmax=23.53 Tsd=0.04  n=24  [ok]
```

Sample 23 carried a simulated +6 °C glitch; the median-of-3 stage removed it completely (`Tmax` is unchanged).
QEMU does not model the reset flags, hence `reset=unknown`; on the Nucleo it reads `power-on`, `pin`, `iwdg`, ….

## 60-second quick start

Requirements: CMake ≥ 3.21 (for presets; the project itself needs ≥ 3.16), Ninja, GCC or Clang, Python 3, `gcc-arm-none-eabi`, `libnewlib-arm-none-eabi`, `libstdc++-arm-none-eabi-newlib`. QEMU is optional.

```bash
cmake --preset host && cmake --build --preset host && ctest --preset host
```

```bash
./build/host/sim/sn_sim --duration-ms 3000 | ./build/host/tools/sn_decode --port -
```

```bash
cmake --preset firmware && cmake --build --preset firmware
```

```bash
scripts/qemu_run.sh 5
```

Flash the Nucleo-F401RE (bootloader + slot A in one image) with either tool:

```bash
st-flash --reset write build/firmware/firmware/sn_full_a.bin 0x08000000
```

```bash
openocd -f board/st_nucleo_f4.cfg -c "program build/firmware/firmware/sn_full_a.bin 0x08000000 verify reset exit"
```

Watch telemetry, change the rate, update the firmware over the same cable:

```bash
./build/host/tools/sn_decode --port /dev/ttyACM0 --send version --send period=50
```

```bash
./build/host/tools/sn_upload --port /dev/ttyACM0 --image-a build/firmware/firmware/sn_app_a.bin --image-b build/firmware/firmware/sn_app_b.bin
```

## Hardware

| Qty | Part | Notes |
|---|---|---|
| 1 | NUCLEO-F401RE | ST-LINK/V2-1 provides SWD, 8 MHz MCO (HSE bypass) and the virtual COM port |
| 1 | BME280 breakout (3.3 V) | I2C address 0x76 (SDO → GND); SPI mode optional |
| 4–6 | Dupont jumpers | |

| Signal | MCU pin | Nucleo header | BME280 (I2C, default) | BME280 (SPI, `-DSN_BME280_BUS=SPI`) |
|---|---|---|---|---|
| 3V3 / GND | – | CN6 3V3 / GND | VIN / GND | VIN / GND |
| I2C1 SCL | PB8 | CN5 D15 | SCL | – |
| I2C1 SDA | PB9 | CN5 D14 | SDA | – |
| SPI2 SCK | PB13 | CN10-30 | – | SCL/SCK |
| SPI2 MISO | PB14 | CN10-28 | – | SDO |
| SPI2 MOSI | PB15 | CN10-26 | – | SDA/SDI |
| SPI2 CS | PB12 | CN10-16 | – | CSB |
| USART2 TX/RX | PA2 / PA3 | ST-LINK VCP | – | – |
| LED LD2 | PA5 | on board | toggles on every watchdog feed | |

Most breakouts already carry 10 kΩ I2C pull-ups; the internal pull-ups are also enabled.

## Requirements and build options

| Tool | Tested version | Needed for |
|---|---|---|
| CMake | 3.28 | everything (≥ 3.16; presets need 3.21) |
| GCC / Clang | 13 / 18 | host build, tests, simulation, tools |
| arm-none-eabi-gcc | 13.2 | firmware |
| Python 3 | 3.11+ | `tools/imgtool.py` (image signing, merging) |
| QEMU | 8.2 | optional: `qemu-system-arm -M netduinoplus2` |

| Option | Default | Effect |
|---|---|---|
| `SN_BUILD_TESTS` | ON | Unit and end-to-end tests (host) |
| `SN_BUILD_SIM` | ON | `sn_sim` — application on the FreeRTOS POSIX port |
| `SN_BUILD_TOOLS` | ON | `sn_decode`, `sn_upload` |
| `SN_SANITIZE` | empty | e.g. `address,undefined` |
| `SN_COVERAGE` | OFF | gcov instrumentation |
| `SN_WERROR` | OFF (ON in presets) | Warnings are errors |
| `SN_BME280_BUS` | `I2C` | `SPI` selects SPI2 wiring on the Nucleo |
| `SN_FREERTOS_TAG` / `SN_CMSIS_CORE_TAG` / `SN_CMSIS_F4_TAG` | `V11.1.0` / `v5.9.0_20250520` / `v2.6.10` | Pinned dependency releases |

Firmware outputs (`build/firmware/firmware/`): `sn_bootloader.{elf,bin,hex}`, `sn_app_a.*`, `sn_app_b.*` (signed, per slot), `sn_full_a.bin` (bootloader + slot A), `sn_app_qemu.*`, `sn_full_qemu.bin`, `*.map`.

## Tasks and priorities

| Task | Prio | Period / trigger | Stack | Job |
|---|---|---|---|---|
| `sample` | 5 | `vTaskDelayUntil`, 10–1000 ms (default 100) | 384 w | BME280 burst read + compensation |
| `filter` | 4 | raw-sample queue (8) | 384 w | median-3 → EMA per channel, 32-sample min/max/σ |
| `cmd` | 3 | UART RX ISR notification, 50 ms poll | 512 w | frame decode, command dispatch |
| `telem` | 2 | telemetry queue (8) | 384 w | encode + frame + DMA TX |
| `health` | 1 | 250 ms | 384 w | watchdog feed, image confirm, health report (1 s) |

`health` feeds the IWDG (2 s) only if **every** task checked in within 1.5 s, so a stalled task resets the node even though the feeder still runs. After 10 s of continuous health a trial image is confirmed. ISRs (USART2, DMA1 Stream6) run at NVIC priority 6, below `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5).

## Memory map

| Region | Address | Size | Content |
|---|---|---|---|
| Sector 0 | `0x0800_0000` | 16 KiB | Bootloader (7.3 KiB used) |
| Sector 1 | `0x0800_4000` | 16 KiB | Boot journal (1024 × 16-byte records) |
| Sectors 2–4 | `0x0800_8000` | 96 KiB | Reserved |
| Sector 5 | `0x0802_0000` | 128 KiB | **Slot A**: 512 B header, vectors at `+0x200` (19.7 KiB used) |
| Sector 6 | `0x0804_0000` | 128 KiB | **Slot B**: same layout, linked for this address |
| Sector 7 | `0x0806_0000` | 128 KiB | Reserved |
| SRAM | `0x2000_0000` | 96 KiB − 32 B | `.data`, `.bss`, static task stacks, main stack (12.3 KiB used) |
| SRAM tail | `0x2001_7FE0` | 32 B | Bootloader ↔ app mailbox (`UPDT` request), not initialised |

Image header (`common/include/sn/image.h`): magic `SNIM`, header version, load address, payload size, payload CRC-32, semantic version, git hash, header CRC-32. The bootloader also checks that the initial SP/PC point into RAM/the image.

**Boot flow:** validate both slots → read the latest journal record → a *pending* slot gets up to 3 trial boots (counter persisted before each jump) → afterwards it is dropped and the confirmed slot boots with `ROLLED_BACK` set. The app confirms itself after 10 s of health.

## Telemetry and command frames

Wire format: `COBS( type | seq | body | CRC-16/CCITT-FALSE LE ) 0x00`, all fields little-endian.

| Off | TELEMETRY `0x01` | Type | Unit |
|---|---|---|---|
| 2 | uptime | u32 | ms |
| 6 | temperature (filtered) | i16 | 0.01 °C |
| 8 | pressure (filtered) | u32 | Pa |
| 12 | humidity (filtered) | u16 | 0.01 %RH |
| 14 / 16 / 18 | temperature min / max / σ (32 samples) | i16 / i16 / u16 | 0.01 °C |
| 20 | sampling period | u16 | ms |
| 22 | status: SENSOR_FAULT, TRIAL, SLOT_B, ROLLED_BACK, TX_DROPS | u16 | bits 0–4 |
| 24 | sample count | u32 | – |

| Type | Direction | Body |
|---|---|---|
| `0x02` HEALTH | node → host | uptime, CPU load ‰, min stack headroom, reset cause, boot flags, rx/frame/tx/sensor errors, watchdog-late count |
| `0x03` BOOT | node → host | version, slot, trial attempts, git hash, reset cause, boot flags |
| `0x10` GET_VERSION | host → node | – → `0x20` VERSION |
| `0x11` SET_PERIOD | host → node | u16 ms (10…1000) → `0x21` ACK |
| `0x12` REBOOT / `0x13` BOOTLOADER | host → node | – → ACK, then reset |
| `0x40`–`0x44` HELLO, BEGIN, DATA, END, BOOT | host → bootloader | update protocol → `0x50` INFO / `0x51` RESULT |

## Architecture

```text
            ┌──────────────────────── application (C++17, FreeRTOS) ─────────────────────────┐
 BME280 ──► │ sample ──queue──► filter ──queue──► telem ──┐                                   │
 (I2C/SPI)  │   ▲ period                                   ├─► frame (CRC16+COBS) ─► UART TX ─┼─► host
            │   └──────────── cmd ◄── frame decoder ◄──────┘◄──── UART RX ring ◄── IRQ ◄─────┼── sn_decode
            │ health: check-ins → IWDG feed, image confirm, HEALTH report                    │
            └───────────────────────────────────── board.h ───────────────────────────────────┘
   board_nucleo.c (84 MHz, DMA, I2C/SPI)  │  board_qemu.c (polled TX, simulated BME280)  │  board_sim.c (Linux)
            └──── drivers (C11, register level) ──── common (C11): crc, cobs, frame, ringbuf, bme280, image, boot_control, update_server
 Bootloader (C11): journal + decide → jump | update_server ◄─ frames ─► sn_upload (C++17)
```

| Decision | Alternative | Why |
|---|---|---|
| COBS + CRC-16 framing | SLIP, length-prefix | Self-synchronising after any byte loss, bounded overhead (≤ 1 byte / 254) |
| Append-only boot journal | Fixed record + erase | No erase on the hot path; a torn write only loses the newest record |
| Two images linked per slot | Position-independent code | Simpler, smaller code; the uploader picks the image for the target slot |
| Stop-and-wait update with cached replies | Sliding window | ~5 KiB/s is ample for 20 KiB images; retries are trivially safe |
| Polled I2C with µs timeouts + bus recovery | IRQ/DMA I2C | 8-byte burst at 400 kHz takes ~250 µs; determinism over throughput |
| Integer filters/statistics | float | Bit-identical results on target, QEMU and host tests |
| Static allocation only | heap_4 | No fragmentation; RAM usage is fixed at link time |

## Testing

| Suite | Covers |
|---|---|
| `test_codec` | CRC check values, COBS reference vectors + 300 random round-trips, stream decoder errors, ring buffer wrap |
| `test_boot` | Image validation failures, journal append/compaction/torn records, A/B decision matrix, rollback |
| `test_update` | Uploader ↔ bootloader server end-to-end over real framing, lost replies, conflicts, flash faults |
| `test_bme280` | Datasheet example (25.08 °C, 100653 Pa), float reference formulas, simulator round-trip, bus faults |
| `test_app` | Median/EMA/window statistics vs brute force, processing, protocol codecs, commands, health supervisor |
| `test_tools`, `imgtool_*` | Decoder formatting, command parsing, `imgtool.py` ↔ C validator cross-check |
| `sim_end_to_end` | Whole application in `sn_sim`: commands in, decoded telemetry out, rate change verified |

```bash
cmake --preset host-asan && cmake --build --preset host-asan && ctest --preset host-asan
```

CI runs the firmware build with a size report, host tests with GCC and Clang, ASan/UBSan, a QEMU boot test, clang-tidy (bugprone, cert, cppcoreguidelines, readability) and cppcheck as hard gates, a MISRA C:2012 addon report, CodeQL and gcovr coverage published to [GitHub Pages](https://bekirhankarakurt.github.io/stm32-freertos-sensor-node/coverage/).

```bash
scripts/static_analysis.sh build/host build/firmware
```

## Troubleshooting / FAQ

| Symptom | Fix |
|---|---|
| No output on `/dev/ttyACM0` | Use 115200 8N1; add yourself to `dialout`; the bootloader waits 200 ms after reset before starting the app |
| `SENSOR_FAULT` in telemetry | Check wiring and address (0x76 vs 0x77 in `board_nucleo.c`); the node retries every second |
| Node resets every ~2 s | A task stopped checking in; `HEALTH wdt_late` counts the late cycles before the reset |
| `sn_upload`: no reply | Close other programs using the port; the uploader first asks the app for the bootloader, then repeats HELLO for 10 s |
| `ROLLED_BACK` after an update | The new image failed to stay healthy for 10 s in three attempts; the previous slot is running again |
| Flashing only `sn_app_a.bin` does nothing | The CPU starts at `0x08000000`: flash `sn_full_a.bin` once (or the bootloader), later only slot images |
| QEMU shows `reset=unknown`, no update | QEMU does not model RCC flags or flash programming; use the simulation/tests for those paths |

## Project tree

```text
app/            portable C++17 logic (protocol, filters, health, commands) + rtos/ task wiring and board.h
common/         portable C11: CRC, COBS, framing, ring buffer, BME280 + simulator, image, boot journal, update server
firmware/       target build: drivers/ (register level), board/ (Nucleo, QEMU), bootloader/, startup/, linker/, config/
sim/            Linux board + FreeRTOS POSIX configuration (sn_sim)
tools/          sn_decode, sn_upload (C++17), imgtool.py (sign/merge/info)
tests/          dependency-free unit tests and the simulation end-to-end test
scripts/        qemu_run.sh, static_analysis.sh, coverage_badge.py
docs/           Doxyfile, Pages landing page, demo.tape (VHS) and demo.gif
cmake/          toolchain file, warnings, dependencies, version stamping
```

## Roadmap

- [x] Register-level drivers, FreeRTOS pipeline, telemetry and commands
- [x] A/B bootloader with trial boots, rollback and UART updates
- [x] Linux simulation, QEMU boot, host tests with sanitizers in CI
- [ ] Ed25519 image signatures (authenticity on top of integrity)
- [ ] Persisted configuration in the reserved sectors
- [ ] Low-power idle (tickless) with stop mode between samples

## License

MIT — see [LICENSE](LICENSE).

Bekirhan Karakurt - [github.com/bekirhankarakurt](https://github.com/bekirhankarakurt)
