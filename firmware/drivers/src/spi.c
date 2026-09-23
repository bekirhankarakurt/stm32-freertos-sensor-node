/**
 * @file spi.c
 * @brief SPI2 master, mode 3, polled full-duplex transfers with bounded waits.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

#define SPI_TIMEOUT_US 1000U
#define PORT_B 1U
#define CS_PIN 12U

static bool wait_flag(uint32_t mask)
{
    const uint32_t start = sn_timebase_us();
    while ((SPI2->SR & mask) == 0U) {
        if ((sn_timebase_us() - start) > SPI_TIMEOUT_US) {
            return false;
        }
    }
    return true;
}

void sn_spi2_init(void)
{
    const sn_gpio_cfg_t af5 = {SN_GPIO_AF, false, false, 5U};
    const sn_gpio_cfg_t cs = {SN_GPIO_OUTPUT, false, false, 0U};
    sn_gpio_write(PORT_B, CS_PIN, true);
    sn_gpio_config(PORT_B, CS_PIN, &cs);
    sn_gpio_config(PORT_B, 13U, &af5);
    sn_gpio_config(PORT_B, 14U, &af5);
    sn_gpio_config(PORT_B, 15U, &af5);
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    __DSB();
    /* Master, fPCLK/8, CPOL = CPHA = 1, software NSS. */
    SPI2->CR1 = SPI_CR1_MSTR | (2U << SPI_CR1_BR_Pos) | SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI2->CR1 |= SPI_CR1_SPE;
}

bool sn_spi2_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    bool ok = true;
    sn_gpio_write(PORT_B, CS_PIN, false);
    for (size_t i = 0U; (i < len) && ok; ++i) {
        ok = wait_flag(SPI_SR_TXE);
        if (ok) {
            *(volatile uint8_t *)&SPI2->DR = (tx != NULL) ? tx[i] : 0xFFU;
            ok = wait_flag(SPI_SR_RXNE);
        }
        if (ok) {
            const uint8_t b = *(volatile uint8_t *)&SPI2->DR;
            if (rx != NULL) {
                rx[i] = b;
            }
        }
    }
    const uint32_t start = sn_timebase_us();
    while (((SPI2->SR & SPI_SR_BSY) != 0U) && ((sn_timebase_us() - start) < SPI_TIMEOUT_US)) {
    }
    sn_gpio_write(PORT_B, CS_PIN, true);
    return ok;
}
