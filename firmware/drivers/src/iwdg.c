/**
 * @file iwdg.c
 * @brief Independent watchdog (LSI ~32 kHz, prescaler 64 -> 2 ms per count).
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

#define IWDG_KEY_RELOAD 0xAAAAU
#define IWDG_KEY_ENABLE 0xCCCCU
#define IWDG_KEY_ACCESS 0x5555U
#define IWDG_SR_TIMEOUT 100000U

void sn_iwdg_start(uint32_t timeout_ms)
{
    uint32_t reload = timeout_ms / 2U;
    reload = (reload == 0U) ? 1U : ((reload > 0xFFFU) ? 0xFFFU : reload);
    IWDG->KR = IWDG_KEY_ENABLE;
    IWDG->KR = IWDG_KEY_ACCESS;
    IWDG->PR = 4U; /* /64 */
    IWDG->RLR = reload;
    for (uint32_t i = 0U; (i < IWDG_SR_TIMEOUT) && (IWDG->SR != 0U); ++i) {
    }
    IWDG->KR = IWDG_KEY_RELOAD;
}

void sn_iwdg_feed(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}
