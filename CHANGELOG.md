# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.0.0] - 2026-11-04

### Added
- Register-level STM32F401 drivers: RCC (HSE bypass with HSI fallback), GPIO, USART2 with DMA TX and interrupt RX ring buffers, I2C1 with bus recovery, SPI2, TIM2 timebase, IWDG and flash.
- FreeRTOS V11.1.0 application with five statically allocated tasks: fixed-rate sampling, median/EMA filtering with window statistics, telemetry, command handling and a task-aware watchdog.
- BME280 driver with the official Bosch integer compensation over I2C or SPI, plus a register-level simulator.
- CRC-16 + COBS framed binary protocol for telemetry, health, boot announcements and commands (version, sampling period, reboot, bootloader).
- A/B bootloader with CRC-32 image headers, append-only boot journal, three trial boots with automatic rollback and a UART update protocol.
- Host tools: `sn_decode` (telemetry decoder and command sender), `sn_upload` (firmware uploader) and `imgtool.py` (sign, merge, inspect).
- Linux simulation on the FreeRTOS POSIX port and a QEMU (netduinoplus2) firmware variant.
- Unit, end-to-end and emulation tests; CI with GCC, Clang, ASan/UBSan, static analysis, CodeQL, coverage and API documentation.

[Unreleased]: https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/bekirhankarakurt/stm32-freertos-sensor-node/releases/tag/v1.0.0
