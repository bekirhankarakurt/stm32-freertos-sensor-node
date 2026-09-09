/**
 * @file frame.h
 * @brief Link-layer framing: payload || CRC-16 (LE) -> COBS -> 0x00 delimiter.
 *
 * The same framing carries telemetry, commands and the bootloader update
 * protocol, so one decoder serves all host tools.
 */
#ifndef SN_FRAME_H
#define SN_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sn/cobs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Largest payload (type + seq + body) carried by one frame. */
#define SN_FRAME_MAX_PAYLOAD 300U
/** Size of the trailing CRC-16. */
#define SN_FRAME_CRC_SIZE 2U
/** Largest COBS block on the wire (without delimiter). */
#define SN_FRAME_MAX_BLOCK SN_COBS_MAX_ENCODED(SN_FRAME_MAX_PAYLOAD + SN_FRAME_CRC_SIZE)
/** Largest frame on the wire including the 0x00 delimiter. */
#define SN_FRAME_MAX_WIRE (SN_FRAME_MAX_BLOCK + 1U)

/** Result of feeding one byte to the stream decoder. */
typedef enum {
    SN_FRAME_NONE = 0,     /**< No complete frame yet. */
    SN_FRAME_OK,           /**< A valid payload was produced. */
    SN_FRAME_ERR_COBS,     /**< Malformed COBS block. */
    SN_FRAME_ERR_CRC,      /**< CRC mismatch. */
    SN_FRAME_ERR_SHORT,    /**< Block too short to hold a CRC. */
    SN_FRAME_ERR_OVERFLOW  /**< Block exceeded ::SN_FRAME_MAX_BLOCK. */
} sn_frame_status_t;

/** Incremental stream decoder state. */
typedef struct {
    uint8_t block[SN_FRAME_MAX_BLOCK]; /**< Bytes since the last delimiter. */
    size_t len;                        /**< Bytes in @ref block. */
    bool overflow;                     /**< Current block overflowed. */
} sn_frame_decoder_t;

/**
 * @brief Encode a payload into a delimited wire frame.
 * @return Bytes written (including delimiter), or 0 if the payload is too
 *         large or @p cap is insufficient.
 */
size_t sn_frame_encode(const uint8_t *payload, size_t len, uint8_t *out, size_t cap);

/** @brief Reset a stream decoder. */
void sn_frame_decoder_init(sn_frame_decoder_t *dec);

/**
 * @brief Feed one received byte.
 * @param dec         Decoder state.
 * @param byte        Received byte.
 * @param payload     Destination for a completed payload.
 * @param cap         Capacity of @p payload.
 * @param payload_len Set to the payload length when ::SN_FRAME_OK is returned.
 */
sn_frame_status_t sn_frame_decoder_push(sn_frame_decoder_t *dec, uint8_t byte, uint8_t *payload,
                                        size_t cap, size_t *payload_len);

#ifdef __cplusplus
}
#endif

#endif /* SN_FRAME_H */
