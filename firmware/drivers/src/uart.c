/**
 * @file uart.c
 * @brief USART2: interrupt-driven RX ring, DMA1 Stream6/Channel4 TX ring, or fully polled mode.
 */
#include "sn/drv.h"
#include "sn/ringbuf.h"
#include "stm32f4xx.h"

#define RX_SIZE 512U
#define TX_SIZE 1024U
#define UART_IRQ_PRIORITY 6U /* numerically above configMAX_SYSCALL (5): may call FreeRTOS FromISR APIs */

static uint8_t g_rx_storage[RX_SIZE];
static uint8_t g_tx_storage[TX_SIZE];
static sn_ringbuf_t g_rx;
static sn_ringbuf_t g_tx;
static volatile uint32_t g_dma_len;
static volatile uint32_t g_errors;
static sn_uart_rx_cb_t g_rx_cb;
static bool g_tx_dma;

/* Interrupt handlers referenced from the vector table. */
void USART2_IRQHandler(void);
void DMA1_Stream6_IRQHandler(void);

void sn_uart2_init(uint32_t pclk_hz, uint32_t baud, bool tx_dma)
{
    const sn_gpio_cfg_t af7 = {SN_GPIO_AF, false, true, 7U};
    sn_gpio_config(0U, 2U, &af7);
    sn_gpio_config(0U, 3U, &af7);
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    __DSB();

    (void)sn_ringbuf_init(&g_rx, g_rx_storage, RX_SIZE);
    (void)sn_ringbuf_init(&g_tx, g_tx_storage, TX_SIZE);
    g_dma_len = 0U;
    g_errors = 0U;
    g_tx_dma = tx_dma;

    USART2->CR1 = 0U;
    USART2->BRR = (pclk_hz + (baud / 2U)) / baud; /* 16x oversampling */
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    if (tx_dma) {
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        __DSB();
        DMA1_Stream6->CR = 0U;
        DMA1_Stream6->PAR = (uint32_t)&USART2->DR;
        USART2->CR3 |= USART_CR3_DMAT;
        NVIC_SetPriority(DMA1_Stream6_IRQn, UART_IRQ_PRIORITY);
        NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    }
    NVIC_SetPriority(USART2_IRQn, UART_IRQ_PRIORITY);
}

void sn_uart2_enable_rx_irq(void)
{
    USART2->CR1 |= USART_CR1_RXNEIE;
    NVIC_EnableIRQ(USART2_IRQn);
}

/* Start the next DMA chunk if the channel is idle; caller masks interrupts. */
static void tx_kick(void)
{
    if (g_dma_len != 0U) {
        return;
    }
    const uint8_t *chunk = NULL;
    const uint32_t n = sn_ringbuf_peek_linear(&g_tx, &chunk);
    if (n == 0U) {
        return;
    }
    g_dma_len = n;
    DMA1->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6;
    DMA1_Stream6->M0AR = (uint32_t)chunk;
    DMA1_Stream6->NDTR = n;
    DMA1_Stream6->CR = (4U << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

void sn_uart2_write_blocking(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; ++i) {
        while ((USART2->SR & USART_SR_TXE) == 0U) {
        }
        USART2->DR = data[i];
    }
    while ((USART2->SR & USART_SR_TC) == 0U) {
    }
}

bool sn_uart2_write(const uint8_t *data, uint32_t len)
{
    if (!g_tx_dma) {
        sn_uart2_write_blocking(data, len);
        return true;
    }
    if (sn_ringbuf_free(&g_tx) < len) {
        return false;
    }
    (void)sn_ringbuf_write(&g_tx, data, len);
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    tx_kick();
    __set_PRIMASK(primask);
    return true;
}

void DMA1_Stream6_IRQHandler(void)
{
    const uint32_t flags = DMA1->HISR;
    DMA1->HIFCR = DMA_HIFCR_CTCIF6 | DMA_HIFCR_CTEIF6;
    if ((flags & (DMA_HISR_TCIF6 | DMA_HISR_TEIF6)) != 0U) {
        if ((flags & DMA_HISR_TEIF6) != 0U) {
            ++g_errors;
        }
        sn_ringbuf_consume(&g_tx, g_dma_len);
        g_dma_len = 0U;
        tx_kick();
    }
}

void USART2_IRQHandler(void)
{
    const uint32_t sr = USART2->SR;
    if ((sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
        /* Reading DR after SR clears RXNE and the error flags. */
        const uint8_t byte = (uint8_t)(USART2->DR & 0xFFU);
        if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U) {
            ++g_errors;
        }
        if (!sn_ringbuf_put(&g_rx, byte)) {
            ++g_errors;
        }
        if (g_rx_cb != NULL) {
            g_rx_cb();
        }
    }
}

uint32_t sn_uart2_read(uint8_t *dst, uint32_t cap)
{
    return sn_ringbuf_read(&g_rx, dst, cap);
}

int32_t sn_uart2_getc(void)
{
    const uint32_t sr = USART2->SR;
    if ((sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) != 0U) {
        (void)USART2->DR;
        ++g_errors;
        return -1;
    }
    if ((sr & USART_SR_RXNE) == 0U) {
        return -1;
    }
    return (int32_t)(USART2->DR & 0xFFU);
}

void sn_uart2_set_rx_callback(sn_uart_rx_cb_t cb)
{
    g_rx_cb = cb;
}

uint32_t sn_uart2_errors(void)
{
    return g_errors;
}
