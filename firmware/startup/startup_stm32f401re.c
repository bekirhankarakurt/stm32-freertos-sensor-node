/**
 * @file startup_stm32f401re.c
 * @brief Vector table and reset handler for the STM32F401RE (shared by bootloader and application).
 */
#include <stdint.h>

#include "stm32f4xx.h"

/* Linker-script symbols; only their addresses are meaningful. */
extern uint32_t _sidata[];
extern uint32_t _sdata[];
extern uint32_t _edata[];
extern uint32_t _sbss[];
extern uint32_t _ebss[];
extern uint32_t _estack[];

int main(void);

typedef void (*init_fn_t)(void);
extern init_fn_t __preinit_array_start[];
extern init_fn_t __preinit_array_end[];
extern init_fn_t __init_array_start[];
extern init_fn_t __init_array_end[];

void Reset_Handler(void);
void Default_Handler(void);

/* Handlers a program may override; all others trap in Default_Handler. */
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DMA1_Stream6_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));
void USART2_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

typedef void (*vector_t)(void);

/** Cortex-M4 core vectors followed by the 85 STM32F401 interrupt vectors. */
__attribute__((section(".isr_vector"), used)) const vector_t g_vector_table[16 + 85] = {
    (vector_t)(uintptr_t)_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
    Default_Handler, /* IRQ 0 */
    Default_Handler, /* IRQ 1 */
    Default_Handler, /* IRQ 2 */
    Default_Handler, /* IRQ 3 */
    Default_Handler, /* IRQ 4 */
    Default_Handler, /* IRQ 5 */
    Default_Handler, /* IRQ 6 */
    Default_Handler, /* IRQ 7 */
    Default_Handler, /* IRQ 8 */
    Default_Handler, /* IRQ 9 */
    Default_Handler, /* IRQ 10 */
    Default_Handler, /* IRQ 11 */
    Default_Handler, /* IRQ 12 */
    Default_Handler, /* IRQ 13 */
    Default_Handler, /* IRQ 14 */
    Default_Handler, /* IRQ 15 */
    Default_Handler, /* IRQ 16 */
    DMA1_Stream6_IRQHandler, /* IRQ 17 */
    Default_Handler, /* IRQ 18 */
    Default_Handler, /* IRQ 19 */
    Default_Handler, /* IRQ 20 */
    Default_Handler, /* IRQ 21 */
    Default_Handler, /* IRQ 22 */
    Default_Handler, /* IRQ 23 */
    Default_Handler, /* IRQ 24 */
    Default_Handler, /* IRQ 25 */
    Default_Handler, /* IRQ 26 */
    Default_Handler, /* IRQ 27 */
    Default_Handler, /* IRQ 28 */
    Default_Handler, /* IRQ 29 */
    Default_Handler, /* IRQ 30 */
    Default_Handler, /* IRQ 31 */
    Default_Handler, /* IRQ 32 */
    Default_Handler, /* IRQ 33 */
    Default_Handler, /* IRQ 34 */
    Default_Handler, /* IRQ 35 */
    Default_Handler, /* IRQ 36 */
    Default_Handler, /* IRQ 37 */
    USART2_IRQHandler, /* IRQ 38 */
    Default_Handler, /* IRQ 39 */
    Default_Handler, /* IRQ 40 */
    Default_Handler, /* IRQ 41 */
    Default_Handler, /* IRQ 42 */
    Default_Handler, /* IRQ 43 */
    Default_Handler, /* IRQ 44 */
    Default_Handler, /* IRQ 45 */
    Default_Handler, /* IRQ 46 */
    Default_Handler, /* IRQ 47 */
    Default_Handler, /* IRQ 48 */
    Default_Handler, /* IRQ 49 */
    Default_Handler, /* IRQ 50 */
    Default_Handler, /* IRQ 51 */
    Default_Handler, /* IRQ 52 */
    Default_Handler, /* IRQ 53 */
    Default_Handler, /* IRQ 54 */
    Default_Handler, /* IRQ 55 */
    Default_Handler, /* IRQ 56 */
    Default_Handler, /* IRQ 57 */
    Default_Handler, /* IRQ 58 */
    Default_Handler, /* IRQ 59 */
    Default_Handler, /* IRQ 60 */
    Default_Handler, /* IRQ 61 */
    Default_Handler, /* IRQ 62 */
    Default_Handler, /* IRQ 63 */
    Default_Handler, /* IRQ 64 */
    Default_Handler, /* IRQ 65 */
    Default_Handler, /* IRQ 66 */
    Default_Handler, /* IRQ 67 */
    Default_Handler, /* IRQ 68 */
    Default_Handler, /* IRQ 69 */
    Default_Handler, /* IRQ 70 */
    Default_Handler, /* IRQ 71 */
    Default_Handler, /* IRQ 72 */
    Default_Handler, /* IRQ 73 */
    Default_Handler, /* IRQ 74 */
    Default_Handler, /* IRQ 75 */
    Default_Handler, /* IRQ 76 */
    Default_Handler, /* IRQ 77 */
    Default_Handler, /* IRQ 78 */
    Default_Handler, /* IRQ 79 */
    Default_Handler, /* IRQ 80 */
    Default_Handler, /* IRQ 81 */
    Default_Handler, /* IRQ 82 */
    Default_Handler, /* IRQ 83 */
    Default_Handler, /* IRQ 84 */
};

void Reset_Handler(void)
{
    /* Enable the FPU before any code that might use it. */
    SCB->CPACR |= (0xFU << 20);
    __DSB();
    __ISB();

    /* Sizes from address differences: the symbols are distinct objects to the compiler. */
    const uintptr_t data_words = ((uintptr_t)_edata - (uintptr_t)_sdata) / sizeof(uint32_t);
    for (uintptr_t i = 0U; i < data_words; ++i) {
        _sdata[i] = _sidata[i];
    }
    const uintptr_t bss_words = ((uintptr_t)_ebss - (uintptr_t)_sbss) / sizeof(uint32_t);
    for (uintptr_t i = 0U; i < bss_words; ++i) {
        _sbss[i] = 0U;
    }
    /* Works whether started by the bootloader or directly by a debugger. */
    SCB->VTOR = (uint32_t)(uintptr_t)g_vector_table;
    __DSB();
    /* Static constructors (no C library start files are linked). */
    const uintptr_t preinit = ((uintptr_t)__preinit_array_end - (uintptr_t)__preinit_array_start) / sizeof(init_fn_t);
    for (uintptr_t i = 0U; i < preinit; ++i) {
        __preinit_array_start[i]();
    }
    const uintptr_t init = ((uintptr_t)__init_array_end - (uintptr_t)__init_array_start) / sizeof(init_fn_t);
    for (uintptr_t i = 0U; i < init; ++i) {
        __init_array_start[i]();
    }
    (void)main();
    NVIC_SystemReset();
}

/** Unexpected exception: halt under a debugger, otherwise reset. */
void Default_Handler(void)
{
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0U) {
        __BKPT(0);
    }
    NVIC_SystemReset();
}
