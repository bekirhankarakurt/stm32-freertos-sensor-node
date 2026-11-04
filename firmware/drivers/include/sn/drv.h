/**
 * @file drv.h
 * @brief Register-level STM32F401 drivers (no HAL): clocks, GPIO, UART, I2C, SPI,
 *        timebase, independent watchdog, flash and reset handling.
 */
#ifndef SN_DRV_H
#define SN_DRV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sn/flash_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Clocks ------------------------------------------------------------- */

/** Core clock in Hz (CMSIS convention). */
extern uint32_t SystemCoreClock;

/** Clock source actually selected by sn_rcc_init_84mhz(). */
typedef enum {
    SN_CLOCK_HSI = 0,     /**< 16 MHz internal oscillator (fallback). */
    SN_CLOCK_PLL_HSI,     /**< 84 MHz from HSI via PLL. */
    SN_CLOCK_PLL_HSE      /**< 84 MHz from the 8 MHz ST-LINK MCO (HSE bypass). */
} sn_clock_source_t;

/**
 * @brief Configure SYSCLK = 84 MHz, APB1 = 42 MHz, APB2 = 84 MHz.
 *
 * Tries HSE bypass first, falls back to HSI + PLL, and finally stays on HSI
 * if the PLL never locks. Every wait is bounded.
 */
sn_clock_source_t sn_rcc_init_84mhz(void);

/** @brief APB1 peripheral clock in Hz for the current configuration. */
uint32_t sn_rcc_apb1_hz(void);

/* ---- GPIO --------------------------------------------------------------- */

/** Pin modes (MODER encoding). */
typedef enum { SN_GPIO_INPUT = 0, SN_GPIO_OUTPUT = 1, SN_GPIO_AF = 2, SN_GPIO_ANALOG = 3 } sn_gpio_mode_t;

/** Pin configuration. */
typedef struct {
    sn_gpio_mode_t mode; /**< Mode. */
    bool open_drain;     /**< Open-drain output stage. */
    bool pull_up;        /**< Internal pull-up. */
    uint8_t af;          /**< Alternate function 0..15. */
} sn_gpio_cfg_t;

/** @brief Configure one pin; @p port is 0 for GPIOA, 1 for GPIOB, ... */
void sn_gpio_config(uint8_t port, uint8_t pin, const sn_gpio_cfg_t *cfg);

/** @brief Drive an output pin. */
void sn_gpio_write(uint8_t port, uint8_t pin, bool high);

/* ---- USART2 (ST-LINK virtual COM port, PA2/PA3) ------------------------- */

/** Callback from the receive interrupt. */
typedef void (*sn_uart_rx_cb_t)(void);

/**
 * @brief Initialise USART2 8N1.
 * @param pclk_hz APB1 clock.
 * @param baud    Baud rate.
 * @param tx_dma  true: interrupt RX + DMA TX rings; false: polled TX/RX only.
 */
void sn_uart2_init(uint32_t pclk_hz, uint32_t baud, bool tx_dma);

/** @brief Queue bytes for DMA transmission (all-or-nothing); blocks only in polled mode. */
bool sn_uart2_write(const uint8_t *data, uint32_t len);

/** @brief Transmit bytes by polling and wait until the line is idle. */
void sn_uart2_write_blocking(const uint8_t *data, uint32_t len);

/** @brief Read buffered received bytes (interrupt mode). */
uint32_t sn_uart2_read(uint8_t *dst, uint32_t cap);

/** @brief Poll one received byte; returns -1 when none is available. */
int32_t sn_uart2_getc(void);

/** @brief Install the receive callback (interrupt mode). */
void sn_uart2_set_rx_callback(sn_uart_rx_cb_t cb);

/** @brief Enable the receive interrupt (call once consumers are ready). */
void sn_uart2_enable_rx_irq(void);

/** @brief Framing, noise, overrun and RX-buffer-full errors since init. */
uint32_t sn_uart2_errors(void);

/* ---- I2C1 (PB8 SCL / PB9 SDA, 400 kHz, polled with timeouts) ------------ */

/** @brief Initialise I2C1 as 400 kHz master and recover a stuck bus. */
void sn_i2c1_init(uint32_t pclk_hz);

/** @brief Write one register. */
bool sn_i2c1_write_reg(uint8_t addr7, uint8_t reg, uint8_t value);

/** @brief Burst-read registers. */
bool sn_i2c1_read_regs(uint8_t addr7, uint8_t reg, uint8_t *dst, size_t len);

/* ---- SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI, PB12 CS; mode 3) ------------- */

/** @brief Initialise SPI2 as master at APB1/8. */
void sn_spi2_init(void);

/** @brief Full-duplex transfer with chip select asserted for the whole transaction. */
bool sn_spi2_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

/* ---- Timebase (TIM2, free-running 1 MHz) -------------------------------- */

/** @brief Start TIM2 as a 32-bit microsecond counter. */
void sn_timebase_init(uint32_t timer_clock_hz);

/** @brief Microseconds since sn_timebase_init(). */
uint32_t sn_timebase_us(void);

/* ---- Independent watchdog ---------------------------------------------- */

/** @brief Start the IWDG with a timeout of @p timeout_ms (max 8190 ms). */
void sn_iwdg_start(uint32_t timeout_ms);

/** @brief Reload the IWDG counter. */
void sn_iwdg_feed(void);

/* ---- Flash -------------------------------------------------------------- */

/** @brief Flash driver implementing ::sn_flash_if_t. */
const sn_flash_if_t *sn_flash_driver(void);

/* ---- Reset -------------------------------------------------------------- */

/** Bootloader/application mailbox in the uninitialised RAM tail (survives soft resets). */
extern volatile uint32_t sn_boot_mailbox;

/** @brief Read the RCC reset flags, clear them and return an sn_reset_cause_t value. */
uint8_t sn_reset_cause_latch(void);

#ifdef __cplusplus
}
#endif

#endif /* SN_DRV_H */
