/**
 * @file timebase.c
 * @brief TIM2 as a free-running 32-bit microsecond counter.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

void sn_timebase_init(uint32_t timer_clock_hz)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    __DSB();
    TIM2->CR1 = 0U;
    TIM2->PSC = (timer_clock_hz / 1000000U) - 1U;
    TIM2->ARR = 0xFFFFFFFFU;
    TIM2->CNT = 0U;
    TIM2->EGR = TIM_EGR_UG; /* load the prescaler */
    TIM2->CR1 = TIM_CR1_CEN;
}

uint32_t sn_timebase_us(void)
{
    return TIM2->CNT;
}
