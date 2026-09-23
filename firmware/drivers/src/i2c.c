/**
 * @file i2c.c
 * @brief I2C1 master (STM32F4 "v1" peripheral), polled, every wait bounded by the TIM2 timebase.
 *
 * Follows the read sequences of RM0368 section 18.3.3 for N = 1, N = 2 and N > 2.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

#define I2C_TIMEOUT_US 2000U
#define SCL_PIN 8U
#define SDA_PIN 9U
#define PORT_B 1U

static bool wait_sr1(uint32_t mask)
{
    const uint32_t start = sn_timebase_us();
    while ((I2C1->SR1 & mask) == 0U) {
        if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO)) != 0U) {
            return false;
        }
        if ((sn_timebase_us() - start) > I2C_TIMEOUT_US) {
            return false;
        }
    }
    return true;
}

static void delay_us(uint32_t us)
{
    const uint32_t start = sn_timebase_us();
    while ((sn_timebase_us() - start) < us) {
    }
}

/* Clock out up to nine pulses so a slave stuck mid-byte releases SDA. */
static void bus_recover(void)
{
    const sn_gpio_cfg_t od_out = {SN_GPIO_OUTPUT, true, true, 0U};
    sn_gpio_config(PORT_B, SCL_PIN, &od_out);
    sn_gpio_config(PORT_B, SDA_PIN, &od_out);
    sn_gpio_write(PORT_B, SDA_PIN, true);
    for (uint32_t i = 0U; i < 9U; ++i) {
        sn_gpio_write(PORT_B, SCL_PIN, false);
        delay_us(5U);
        sn_gpio_write(PORT_B, SCL_PIN, true);
        delay_us(5U);
    }
    /* STOP condition: SDA rises while SCL is high. */
    sn_gpio_write(PORT_B, SDA_PIN, false);
    delay_us(5U);
    sn_gpio_write(PORT_B, SDA_PIN, true);
    delay_us(5U);
}

void sn_i2c1_init(uint32_t pclk_hz)
{
    bus_recover();
    const sn_gpio_cfg_t af4 = {SN_GPIO_AF, true, true, 4U};
    sn_gpio_config(PORT_B, SCL_PIN, &af4);
    sn_gpio_config(PORT_B, SDA_PIN, &af4);

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    const uint32_t mhz = pclk_hz / 1000000U;
    I2C1->CR1 = 0U;
    I2C1->CR2 = mhz;
    /* Fast mode, duty 2:1: t_high + t_low = 3 * CCR / f_pclk. */
    uint32_t ccr = pclk_hz / (3U * 400000U);
    ccr = (ccr < 1U) ? 1U : ccr;
    I2C1->CCR = I2C_CCR_FS | ccr;
    I2C1->TRISE = ((mhz * 300U) / 1000U) + 1U;
    I2C1->CR1 = I2C_CR1_PE;
}

static void stop_and_clear(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    I2C1->SR1 = 0U;
}

static bool start_addr(uint8_t addr7, bool read)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB)) {
        return false;
    }
    I2C1->DR = (uint32_t)((uint32_t)addr7 << 1) | (read ? 1U : 0U);
    return wait_sr1(I2C_SR1_ADDR);
}

static void clear_addr(void)
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

static bool send_reg(uint8_t addr7, uint8_t reg)
{
    if (!start_addr(addr7, false)) {
        return false;
    }
    clear_addr();
    I2C1->DR = reg;
    return wait_sr1(I2C_SR1_TXE);
}

bool sn_i2c1_write_reg(uint8_t addr7, uint8_t reg, uint8_t value)
{
    bool ok = send_reg(addr7, reg);
    if (ok) {
        I2C1->DR = value;
        ok = wait_sr1(I2C_SR1_BTF);
    }
    stop_and_clear();
    return ok;
}

static bool read_bytes(uint8_t *dst, size_t len)
{
    if (len == 1U) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;
        if (!wait_sr1(I2C_SR1_RXNE)) {
            return false;
        }
        dst[0] = (uint8_t)I2C1->DR;
        return true;
    }
    if (len == 2U) {
        I2C1->CR1 |= I2C_CR1_POS;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        if (!wait_sr1(I2C_SR1_BTF)) {
            return false;
        }
        I2C1->CR1 |= I2C_CR1_STOP;
        dst[0] = (uint8_t)I2C1->DR;
        dst[1] = (uint8_t)I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;
        return true;
    }
    I2C1->CR1 |= I2C_CR1_ACK;
    clear_addr();
    size_t i = 0U;
    while ((len - i) > 3U) {
        if (!wait_sr1(I2C_SR1_RXNE)) {
            return false;
        }
        dst[i] = (uint8_t)I2C1->DR;
        ++i;
    }
    /* Three bytes left: data N-2 in DR, N-1 in the shift register. */
    if (!wait_sr1(I2C_SR1_BTF)) {
        return false;
    }
    I2C1->CR1 &= ~I2C_CR1_ACK;
    dst[i] = (uint8_t)I2C1->DR;
    if (!wait_sr1(I2C_SR1_BTF)) {
        return false;
    }
    I2C1->CR1 |= I2C_CR1_STOP;
    dst[i + 1U] = (uint8_t)I2C1->DR;
    if (!wait_sr1(I2C_SR1_RXNE)) {
        return false;
    }
    dst[i + 2U] = (uint8_t)I2C1->DR;
    return true;
}

bool sn_i2c1_read_regs(uint8_t addr7, uint8_t reg, uint8_t *dst, size_t len)
{
    if (len == 0U) {
        return true;
    }
    bool ok = send_reg(addr7, reg) && start_addr(addr7, true) && read_bytes(dst, len);
    if (!ok) {
        stop_and_clear();
        I2C1->CR1 &= ~I2C_CR1_POS;
    }
    return ok;
}
