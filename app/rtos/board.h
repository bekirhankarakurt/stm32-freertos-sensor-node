/**
 * @file board.h
 * @brief Hardware boundary between the RTOS application and a concrete board.
 *
 * Implemented by the Nucleo-F401RE board, the QEMU board and the Linux
 * simulation, so the task code is identical in all three.
 */
#ifndef SN_BOARD_H
#define SN_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "sn/bme280.h"
#include "sn/flash_if.h"

#ifdef __cplusplus
#define SN_NORETURN [[noreturn]]
extern "C" {
#else
#define SN_NORETURN _Noreturn
#endif

/** Reset causes (values match sn::proto::ResetCause). */
typedef enum {
    SN_RESET_UNKNOWN = 0,
    SN_RESET_POWER_ON,
    SN_RESET_PIN,
    SN_RESET_SOFTWARE,
    SN_RESET_IWDG,
    SN_RESET_WWDG,
    SN_RESET_LOW_POWER,
    SN_RESET_BROWNOUT
} sn_reset_cause_t;

/** Callback invoked from the UART receive interrupt. */
typedef void (*sn_rx_callback_t)(void);

/** @brief Clocks, pins and peripherals; called once before the scheduler starts. */
void board_init(void);

/** @brief Current core clock in Hz (used by the RTOS tick configuration). */
uint32_t board_core_clock_hz(void);

/** @brief Register-level transport of the environmental sensor. */
const sn_bme280_bus_t *board_sensor_bus(void);

/** @brief Queue a complete frame for transmission; all-or-nothing, never blocks. */
bool board_uart_write(const uint8_t *data, uint32_t len);

/** @brief Drain up to @p cap received bytes. */
uint32_t board_uart_read(uint8_t *dst, uint32_t cap);

/** @brief Install the receive notification callback (may be NULL). */
void board_uart_set_rx_callback(sn_rx_callback_t cb);

/** @brief UART framing/noise/overrun errors since start. */
uint32_t board_uart_rx_errors(void);

/** @brief Start the independent watchdog. */
void board_watchdog_start(uint32_t timeout_ms);

/** @brief Refresh the independent watchdog. */
void board_watchdog_feed(void);

/** @brief Cause of the most recent reset (latched at board_init). */
sn_reset_cause_t board_reset_cause(void);

/** @brief Software reset. */
SN_NORETURN void board_reset(void);

/** @brief Reset into the bootloader's update mode. */
SN_NORETURN void board_enter_bootloader(void);

/** @brief Flash access for the boot journal. */
const sn_flash_if_t *board_flash(void);

/** @brief Slot this image runs from (SN_SLOT_A / SN_SLOT_B). */
uint8_t board_running_slot(void);

/** @brief Free-running microsecond counter (required when configGENERATE_RUN_TIME_STATS is 1). */
uint32_t board_runtime_counter(void);

/** @brief Status LED. */
void board_led(bool on);

#ifdef __cplusplus
}
#endif

#endif /* SN_BOARD_H */
