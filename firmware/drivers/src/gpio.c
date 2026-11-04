/**
 * @file gpio.c
 * @brief GPIO configuration.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

static GPIO_TypeDef *port_regs(uint8_t port)
{
    return (GPIO_TypeDef *)(GPIOA_BASE + ((uint32_t)port * 0x400U));
}

void sn_gpio_config(uint8_t port, uint8_t pin, const sn_gpio_cfg_t *cfg)
{
    RCC->AHB1ENR |= (1U << port);
    __DSB();
    GPIO_TypeDef *g = port_regs(port);
    const uint32_t shift2 = (uint32_t)pin * 2U;
    g->MODER = (g->MODER & ~(3U << shift2)) | ((uint32_t)cfg->mode << shift2);
    g->OTYPER = (g->OTYPER & ~(1U << pin)) | ((cfg->open_drain ? 1U : 0U) << pin);
    g->OSPEEDR |= (2U << shift2); /* fast */
    g->PUPDR = (g->PUPDR & ~(3U << shift2)) | ((cfg->pull_up ? 1U : 0U) << shift2);
    const uint32_t afr_idx = (uint32_t)pin >> 3;
    const uint32_t afr_shift = ((uint32_t)pin & 7U) * 4U;
    g->AFR[afr_idx] = (g->AFR[afr_idx] & ~(0xFU << afr_shift)) | ((uint32_t)cfg->af << afr_shift);
}

void sn_gpio_write(uint8_t port, uint8_t pin, bool high)
{
    port_regs(port)->BSRR = high ? (1U << pin) : (1U << (pin + 16U));
}
