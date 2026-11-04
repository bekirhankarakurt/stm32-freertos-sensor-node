/**
 * @file main.c
 * @brief A/B bootloader: image validation, trial boots with rollback, UART update mode.
 *
 * Runs from the 16 MHz HSI with polled USART2 (115200 8N1) and SysTick as a
 * millisecond timebase. Update mode is entered when no bootable image exists,
 * when the application left the mailbox request, or when a HELLO frame
 * arrives within the short listen window after reset.
 */
#include <stdbool.h>
#include <stdint.h>

#include "sn/boot_control.h"
#include "sn/drv.h"
#include "sn/flash_layout.h"
#include "sn/frame.h"
#include "sn/image.h"
#include "sn/proto_ids.h"
#include "sn/update_server.h"
#include "stm32f4xx.h"

#define BL_VERSION_MAJOR 1U
#define BL_VERSION_MINOR 0U
#define HSI_HZ 16000000U
#define UART_BAUD 115200U
#define LISTEN_WINDOW_MS 200U
#define UPDATE_IDLE_TIMEOUT_MS 30000U

static volatile uint32_t g_ms;

void SysTick_Handler(void);

void SysTick_Handler(void)
{
    ++g_ms;
}

static void send_payload(const uint8_t *payload, size_t len)
{
    uint8_t wire[SN_FRAME_MAX_WIRE];
    const size_t n = sn_frame_encode(payload, len, wire, sizeof(wire));
    sn_uart2_write_blocking(wire, (uint32_t)n);
}

static void jump_to(uint32_t slot_addr)
{
    const uint32_t vectors = slot_addr + SN_IMAGE_HEADER_AREA;
    const uint32_t sp = *(const volatile uint32_t *)vectors;
    const uint32_t pc = *(const volatile uint32_t *)(vectors + 4U);

    __disable_irq();
    SysTick->CTRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    for (uint32_t i = 0U; i < 8U; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    USART2->CR1 = 0U;
    SCB->VTOR = vectors;
    __DSB();
    __ISB();
    __set_MSP(sp);
    __enable_irq();
    ((void (*)(void))(uintptr_t)pc)();
}

/* Wait up to @p window_ms for a HELLO frame; returns true if one arrived. */
static bool listen_for_hello(sn_frame_decoder_t *dec, uint8_t *payload, size_t *len, uint32_t window_ms)
{
    const uint32_t start = g_ms;
    while ((g_ms - start) < window_ms) {
        const int32_t c = sn_uart2_getc();
        if ((c >= 0) && (sn_frame_decoder_push(dec, (uint8_t)c, payload, SN_FRAME_MAX_PAYLOAD, len) == SN_FRAME_OK) &&
            (payload[0] == SN_MSG_UPD_HELLO)) {
            return true;
        }
    }
    return false;
}

/* Serve update requests; returns when the host asks to boot or the link stays idle. */
static void update_mode(sn_update_server_t *srv, sn_frame_decoder_t *dec, const uint8_t *first, size_t first_len,
                        bool may_time_out)
{
    uint8_t payload[SN_FRAME_MAX_PAYLOAD];
    uint8_t resp[SN_UPDATE_MAX_RESPONSE];
    sn_update_action_t action = SN_UPDATE_ACTION_NONE;
    uint32_t last_activity = g_ms;

    if (first_len > 0U) {
        const size_t n = sn_update_server_handle(srv, first, first_len, resp, sizeof(resp), &action);
        send_payload(resp, n);
    }
    while (action != SN_UPDATE_ACTION_BOOT) {
        if (may_time_out && ((g_ms - last_activity) > UPDATE_IDLE_TIMEOUT_MS)) {
            return;
        }
        const int32_t c = sn_uart2_getc();
        if (c < 0) {
            continue;
        }
        size_t len = 0U;
        if (sn_frame_decoder_push(dec, (uint8_t)c, payload, sizeof(payload), &len) == SN_FRAME_OK) {
            last_activity = g_ms;
            const size_t n = sn_update_server_handle(srv, payload, len, resp, sizeof(resp), &action);
            send_payload(resp, n);
        }
    }
}

int main(void)
{
    SystemCoreClock = HSI_HZ;
    (void)SysTick_Config(HSI_HZ / 1000U);
    sn_uart2_init(HSI_HZ, UART_BAUD, false);

    const sn_flash_if_t *flash = sn_flash_driver();
    const sn_update_config_t cfg = {flash,
                                    {flash, SN_JOURNAL_ADDR, SN_JOURNAL_SIZE},
                                    {SN_SLOT_A_ADDR, SN_SLOT_B_ADDR},
                                    SN_SLOT_SIZE,
                                    BL_VERSION_MAJOR,
                                    BL_VERSION_MINOR};
    static sn_update_server_t srv;
    static sn_frame_decoder_t dec;
    sn_update_server_init(&srv, &cfg);
    sn_frame_decoder_init(&dec);

    const bool requested = (sn_boot_mailbox == SN_MAILBOX_ENTER_UPDATE);
    sn_boot_mailbox = 0U;

    static uint8_t first[SN_FRAME_MAX_PAYLOAD];
    size_t first_len = 0U;
    bool serve = requested || listen_for_hello(&dec, first, &first_len, LISTEN_WINDOW_MS);

    for (;;) {
        sn_boot_state_t state;
        uint32_t seq = 0U;
        (void)sn_journal_load(&cfg.journal, &state, &seq);
        sn_image_header_t hdr;
        const bool valid_a = sn_image_validate(flash, SN_SLOT_A_ADDR, SN_SLOT_SIZE, &hdr) == SN_IMAGE_OK;
        const bool valid_b = sn_image_validate(flash, SN_SLOT_B_ADDR, SN_SLOT_SIZE, &hdr) == SN_IMAGE_OK;
        const sn_boot_decision_t d = sn_boot_decide(&state, valid_a, valid_b);

        if (serve || (d.boot_slot == SN_SLOT_NONE)) {
            update_mode(&srv, &dec, first, first_len, d.boot_slot != SN_SLOT_NONE);
            first_len = 0U;
            serve = false;
            continue;
        }
        if (d.persist) {
            /* A failed journal write must not brick the node: boot anyway. */
            (void)sn_journal_store(&cfg.journal, &d.state);
        }
        jump_to((d.boot_slot == SN_SLOT_B) ? SN_SLOT_B_ADDR : SN_SLOT_A_ADDR);
    }
}
