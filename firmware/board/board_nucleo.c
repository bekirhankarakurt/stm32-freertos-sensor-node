/**
 * @file board_nucleo.c
 * @brief Nucleo-F401RE: 84 MHz, USART2 with DMA on the ST-LINK VCP, BME280 on I2C1 or SPI2.
 */
#include "board.h"
#include "board_common.h"
#include "sn/drv.h"

#define BME280_I2C_ADDR 0x76U /* SDO tied to GND; 0x77 when tied to VDDIO */
#define UART_BAUD 115200U

#if defined(SN_BME280_SPI)
static bool bus_read(void *ctx, uint8_t reg, uint8_t *dst, size_t len)
{
    (void)ctx;
    uint8_t tx[1U + 32U] = {0};
    uint8_t rx[1U + 32U];
    if (len > 32U) {
        return false;
    }
    tx[0] = (uint8_t)(reg | 0x80U);
    if (!sn_spi2_transfer(tx, rx, len + 1U)) {
        return false;
    }
    for (size_t i = 0U; i < len; ++i) {
        dst[i] = rx[i + 1U];
    }
    return true;
}

static bool bus_write(void *ctx, uint8_t reg, uint8_t value)
{
    (void)ctx;
    const uint8_t tx[2] = {(uint8_t)(reg & 0x7FU), value};
    return sn_spi2_transfer(tx, NULL, sizeof(tx));
}
#else
static bool bus_read(void *ctx, uint8_t reg, uint8_t *dst, size_t len)
{
    (void)ctx;
    return sn_i2c1_read_regs(BME280_I2C_ADDR, reg, dst, len);
}

static bool bus_write(void *ctx, uint8_t reg, uint8_t value)
{
    (void)ctx;
    return sn_i2c1_write_reg(BME280_I2C_ADDR, reg, value);
}
#endif

static void bus_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    const uint32_t start = sn_timebase_us();
    while ((sn_timebase_us() - start) < (ms * 1000U)) {
    }
}

/* delay_ms is replaced by an RTOS delay in the sampling task. */
static const sn_bme280_bus_t g_bus = {NULL, bus_read, bus_write, bus_delay};

void board_init(void)
{
    (void)sn_rcc_init_84mhz();
    board_common_init();
    const uint32_t apb1 = sn_rcc_apb1_hz();
    /* Timers on APB1 run at twice the bus clock when APB1 is divided. */
    sn_timebase_init((apb1 == SystemCoreClock) ? apb1 : (2U * apb1));
    sn_uart2_init(apb1, UART_BAUD, true);
#if defined(SN_BME280_SPI)
    sn_spi2_init();
#else
    sn_i2c1_init(apb1);
#endif
}

const sn_bme280_bus_t *board_sensor_bus(void)
{
    return &g_bus;
}
