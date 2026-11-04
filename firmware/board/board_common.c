/**
 * @file board_common.c
 * @brief board.h functions shared by the Nucleo and QEMU targets.
 */
#include "board_common.h"

#include "board.h"
#include "sn/boot_control.h"
#include "sn/drv.h"
#include "sn/flash_layout.h"
#include "stm32f4xx.h"

extern uint32_t __image_start;

static sn_reset_cause_t g_reset_cause;

void board_common_init(void)
{
    g_reset_cause = (sn_reset_cause_t)sn_reset_cause_latch();
    const sn_gpio_cfg_t led = {SN_GPIO_OUTPUT, false, false, 0U};
    sn_gpio_config(0U, 5U, &led);
}

uint32_t board_core_clock_hz(void)
{
    return SystemCoreClock;
}

bool board_uart_write(const uint8_t *data, uint32_t len)
{
    return sn_uart2_write(data, len);
}

uint32_t board_uart_read(uint8_t *dst, uint32_t cap)
{
    return sn_uart2_read(dst, cap);
}

void board_uart_set_rx_callback(sn_rx_callback_t cb)
{
    sn_uart2_set_rx_callback(cb);
    sn_uart2_enable_rx_irq();
}

uint32_t board_uart_rx_errors(void)
{
    return sn_uart2_errors();
}

void board_watchdog_start(uint32_t timeout_ms)
{
    sn_iwdg_start(timeout_ms);
}

void board_watchdog_feed(void)
{
    sn_iwdg_feed();
}

sn_reset_cause_t board_reset_cause(void)
{
    return g_reset_cause;
}

void board_reset(void)
{
    NVIC_SystemReset();
}

void board_enter_bootloader(void)
{
    sn_boot_mailbox = SN_MAILBOX_ENTER_UPDATE;
    __DSB();
    NVIC_SystemReset();
}

const sn_flash_if_t *board_flash(void)
{
    return sn_flash_driver();
}

uint8_t board_running_slot(void)
{
    return ((uint32_t)(uintptr_t)&__image_start == SN_SLOT_B_ADDR) ? SN_SLOT_B : SN_SLOT_A;
}

uint32_t board_runtime_counter(void)
{
    return sn_timebase_us();
}

void board_led(bool on)
{
    sn_gpio_write(0U, 5U, on);
}
