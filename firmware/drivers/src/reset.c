/**
 * @file reset.c
 * @brief Reset cause decoding from RCC_CSR.
 */
#include "sn/drv.h"
#include "stm32f4xx.h"

volatile uint32_t sn_boot_mailbox __attribute__((section(".boot_shared")));

/* Values match sn_reset_cause_t in app/rtos/board.h. */
enum { CAUSE_UNKNOWN = 0, CAUSE_POR, CAUSE_PIN, CAUSE_SW, CAUSE_IWDG, CAUSE_WWDG, CAUSE_LPWR, CAUSE_BOR };

uint8_t sn_reset_cause_latch(void)
{
    const uint32_t csr = RCC->CSR;
    uint8_t cause = CAUSE_UNKNOWN;
    if ((csr & RCC_CSR_IWDGRSTF) != 0U) {
        cause = CAUSE_IWDG;
    } else if ((csr & RCC_CSR_WWDGRSTF) != 0U) {
        cause = CAUSE_WWDG;
    } else if ((csr & RCC_CSR_SFTRSTF) != 0U) {
        cause = CAUSE_SW;
    } else if ((csr & RCC_CSR_LPWRRSTF) != 0U) {
        cause = CAUSE_LPWR;
    } else if ((csr & RCC_CSR_PORRSTF) != 0U) {
        cause = CAUSE_POR;
    } else if ((csr & RCC_CSR_BORRSTF) != 0U) {
        cause = CAUSE_BOR;
    } else if ((csr & RCC_CSR_PINRSTF) != 0U) {
        cause = CAUSE_PIN;
    } else {
        cause = CAUSE_UNKNOWN;
    }
    RCC->CSR |= RCC_CSR_RMVF;
    return cause;
}
