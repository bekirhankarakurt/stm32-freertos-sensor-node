/**
 * @file board_sim.c
 * @brief board.h implementation for Linux: stdio UART, simulated BME280, RAM flash.
 */
#define _POSIX_C_SOURCE 200809L

#include "board_sim.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "board.h"
#include "sn/boot_control.h"
#include "sn/bme280_sim.h"
#include "sn/flash_sim.h"

static sn_sim_options_t g_opt = {STDOUT_FILENO, -1, 0U, 1U};
static struct timespec g_start;
static sn_bme280_sim_t g_sensor;
static sn_bme280_bus_t g_sensor_bus;
static sn_env_model_t g_env;
static sn_flash_sim_t g_flash;
static sn_flash_if_t g_flash_if;
static uint32_t g_wdt_timeout_ms;
static uint32_t g_wdt_last_feed_ms;

static uint32_t elapsed_us(void)
{
    struct timespec now;
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    const int64_t us = ((int64_t)(now.tv_sec - g_start.tv_sec) * 1000000) +
                       ((int64_t)(now.tv_nsec - g_start.tv_nsec) / 1000);
    return (uint32_t)us;
}

static void env_provider(void *ctx, sn_env_t *env)
{
    sn_env_model_sample((sn_env_model_t *)ctx, elapsed_us() / 1000U, env);
}

void board_sim_configure(const sn_sim_options_t *opt)
{
    g_opt = *opt;
}

void board_init(void)
{
    (void)clock_gettime(CLOCK_MONOTONIC, &g_start);
    if (g_opt.in_fd >= 0) {
        const int flags = fcntl(g_opt.in_fd, F_GETFL);
        (void)fcntl(g_opt.in_fd, F_SETFL, flags | O_NONBLOCK);
    }
    sn_env_model_init(&g_env, g_opt.seed);
    sn_bme280_sim_init(&g_sensor, env_provider, &g_env);
    g_sensor_bus = sn_bme280_sim_bus(&g_sensor);
    sn_flash_sim_init(&g_flash);
    g_flash_if = sn_flash_sim_if(&g_flash);
}

uint32_t board_core_clock_hz(void)
{
    return 0U;
}

const sn_bme280_bus_t *board_sensor_bus(void)
{
    return &g_sensor_bus;
}

bool board_uart_write(const uint8_t *data, uint32_t len)
{
    uint32_t done = 0U;
    while (done < len) {
        const ssize_t n = write(g_opt.out_fd, &data[done], (size_t)(len - done));
        if (n > 0) {
            done += (uint32_t)n;
        } else if ((n < 0) && (errno == EINTR)) {
            continue;
        } else {
            return false;
        }
    }
    return true;
}

uint32_t board_uart_read(uint8_t *dst, uint32_t cap)
{
    if ((g_opt.in_fd < 0) || (cap == 0U)) {
        return 0U;
    }
    const ssize_t n = read(g_opt.in_fd, dst, (size_t)cap);
    return (n > 0) ? (uint32_t)n : 0U;
}

void board_uart_set_rx_callback(sn_rx_callback_t cb)
{
    /* Host input is polled by the command task; no interrupt source exists. */
    (void)cb;
}

uint32_t board_uart_rx_errors(void)
{
    return 0U;
}

void board_watchdog_start(uint32_t timeout_ms)
{
    g_wdt_timeout_ms = timeout_ms;
    g_wdt_last_feed_ms = elapsed_us() / 1000U;
}

void board_watchdog_feed(void)
{
    g_wdt_last_feed_ms = elapsed_us() / 1000U;
}

sn_reset_cause_t board_reset_cause(void)
{
    return SN_RESET_POWER_ON;
}

void board_reset(void)
{
    (void)fprintf(stderr, "[sim] software reset requested\n");
    _exit(SN_SIM_EXIT_RESET);
}

void board_enter_bootloader(void)
{
    (void)fprintf(stderr, "[sim] bootloader requested\n");
    _exit(SN_SIM_EXIT_BOOTLOADER);
}

const sn_flash_if_t *board_flash(void)
{
    return &g_flash_if;
}

uint8_t board_running_slot(void)
{
    return SN_SLOT_A;
}

void board_led(bool on)
{
    (void)on;
}

void board_sim_idle(void)
{
    const uint32_t now_ms = elapsed_us() / 1000U;
    if ((g_wdt_timeout_ms != 0U) && ((now_ms - g_wdt_last_feed_ms) > g_wdt_timeout_ms)) {
        (void)fprintf(stderr, "[sim] watchdog expired\n");
        _exit(SN_SIM_EXIT_WATCHDOG);
    }
    if ((g_opt.duration_ms != 0U) && (now_ms >= g_opt.duration_ms)) {
        _exit(EXIT_SUCCESS);
    }
}
