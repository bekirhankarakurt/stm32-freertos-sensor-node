/**
 * @file rcc.c
 * @brief Clock tree configuration with bounded waits and fallbacks.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

#define READY_TIMEOUT 200000U

uint32_t SystemCoreClock = 16000000U;
static uint32_t g_apb1_hz = 16000000U;

static bool wait_flag(volatile const uint32_t *reg, uint32_t mask)
{
    for (uint32_t i = 0U; i < READY_TIMEOUT; ++i) {
        if ((*reg & mask) != 0U) {
            return true;
        }
    }
    return false;
}

static bool start_pll(uint32_t source, uint32_t pllm)
{
    RCC->CR &= ~RCC_CR_PLLON;
    /* VCO = in / M * 336 = 336 MHz (VCO input 1 MHz); SYSCLK = VCO / 4; USB = VCO / 7. */
    RCC->PLLCFGR = source | pllm | (336U << RCC_PLLCFGR_PLLN_Pos) | (1U << RCC_PLLCFGR_PLLP_Pos) |
                   (7U << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_flag(&RCC->CR, RCC_CR_PLLRDY)) {
        RCC->CR &= ~RCC_CR_PLLON;
        return false;
    }
    /* 84 MHz at 3.3 V needs 2 wait states; enable prefetch and caches. */
    FLASH->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_SW)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_SW_PLL;
    for (uint32_t i = 0U; i < READY_TIMEOUT; ++i) {
        if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL) {
            SystemCoreClock = 84000000U;
            g_apb1_hz = 42000000U;
            return true;
        }
    }
    return false;
}

sn_clock_source_t sn_rcc_init_84mhz(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR = (PWR->CR & ~PWR_CR_VOS) | PWR_CR_VOS_1; /* scale 2 is sufficient for 84 MHz */

    RCC->CR |= RCC_CR_HSEBYP | RCC_CR_HSEON;
    if (wait_flag(&RCC->CR, RCC_CR_HSERDY) && start_pll(RCC_PLLCFGR_PLLSRC_HSE, 8U)) {
        return SN_CLOCK_PLL_HSE;
    }
    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP);
    if (start_pll(RCC_PLLCFGR_PLLSRC_HSI, 16U)) {
        return SN_CLOCK_PLL_HSI;
    }
    SystemCoreClock = 16000000U;
    g_apb1_hz = 16000000U;
    return SN_CLOCK_HSI;
}

uint32_t sn_rcc_apb1_hz(void)
{
    return g_apb1_hz;
}
