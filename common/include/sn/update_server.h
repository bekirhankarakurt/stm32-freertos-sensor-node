/**
 * @file update_server.h
 * @brief Bootloader side of the firmware update protocol (transport-agnostic).
 *
 * Stop-and-wait: every request gets exactly one response. A retransmitted
 * request (same type and sequence number as the previous one) is answered
 * from a cache without being executed again, and DATA writes are idempotent,
 * so the host can simply retry on timeout.
 */
#ifndef SN_UPDATE_SERVER_H
#define SN_UPDATE_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sn/boot_control.h"
#include "sn/flash_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Action the bootloader must take after sending the response. */
typedef enum {
    SN_UPDATE_ACTION_NONE = 0, /**< Keep serving requests. */
    SN_UPDATE_ACTION_BOOT      /**< Leave update mode and boot. */
} sn_update_action_t;

/** Server configuration. */
typedef struct {
    const sn_flash_if_t *flash; /**< Flash driver. */
    sn_journal_t journal;       /**< Boot journal. */
    uint32_t slot_addr[2];      /**< Slot base addresses. */
    uint32_t slot_size;         /**< Slot capacity. */
    uint8_t bl_major;           /**< Bootloader version major. */
    uint8_t bl_minor;           /**< Bootloader version minor. */
} sn_update_config_t;

/** Largest response produced by the server. */
#define SN_UPDATE_MAX_RESPONSE 20U

/** Server state. */
typedef struct {
    sn_update_config_t cfg;                  /**< Configuration. */
    bool session;                            /**< BEGIN accepted, slot erased. */
    uint8_t target;                          /**< Slot being written. */
    uint32_t total;                          /**< Announced image size. */
    bool has_last;                           /**< @ref last_resp is valid. */
    uint8_t last_type;                       /**< Type of the last request. */
    uint8_t last_seq;                        /**< Sequence of the last request. */
    size_t last_len;                         /**< Length of @ref last_resp. */
    uint8_t last_resp[SN_UPDATE_MAX_RESPONSE]; /**< Cached reply for retransmissions. */
} sn_update_server_t;

/** @brief Initialise a server. */
void sn_update_server_init(sn_update_server_t *srv, const sn_update_config_t *cfg);

/**
 * @brief Handle one decoded request payload.
 * @param req      Request payload (type, seq, body).
 * @param len      Request length.
 * @param resp     Response buffer (>= ::SN_UPDATE_MAX_RESPONSE bytes).
 * @param cap      Response capacity.
 * @param[out] action Follow-up action.
 * @return Response length (0 when @p req is too short to answer).
 */
size_t sn_update_server_handle(sn_update_server_t *srv, const uint8_t *req, size_t len, uint8_t *resp,
                               size_t cap, sn_update_action_t *action);

#ifdef __cplusplus
}
#endif

#endif /* SN_UPDATE_SERVER_H */
