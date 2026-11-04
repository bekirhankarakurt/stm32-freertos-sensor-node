/**
 * @file board_qemu.c
 * @brief QEMU netduinoplus2 (STM32F405 model): polled USART2 TX and the simulated BME280.
 *
 * QEMU does not model RCC, DMA or I2C, so this board keeps the reset clock
 * tree, transmits by polling and drives the real BME280 driver through the
 * register-level simulator. Everything above board.h is identical to the
 * Nucleo build.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "board_common.h"
#include "sn/bme280_sim.h"
#include "sn/drv.h"

/* Clock QEMU feeds to the Cortex-M SysTick/timers of the STM32F405 SoC model. */
#define QEMU_SYSCLK_HZ 168000000U
#define UART_BAUD 115200U

static sn_bme280_sim_t g_sensor;
static sn_bme280_bus_t g_bus;
static sn_env_model_t g_env;

static void env_provider(void *ctx, sn_env_t *env)
{
    const uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    sn_env_model_sample((sn_env_model_t *)ctx, now_ms, env);
}

void board_init(void)
{
    SystemCoreClock = QEMU_SYSCLK_HZ;
    board_common_init();
    sn_timebase_init(QEMU_SYSCLK_HZ);
    sn_uart2_init(QEMU_SYSCLK_HZ / 4U, UART_BAUD, false);
    sn_env_model_init(&g_env, 1U);
    sn_bme280_sim_init(&g_sensor, env_provider, &g_env);
    g_bus = sn_bme280_sim_bus(&g_sensor);
}

const sn_bme280_bus_t *board_sensor_bus(void)
{
    return &g_bus;
}
