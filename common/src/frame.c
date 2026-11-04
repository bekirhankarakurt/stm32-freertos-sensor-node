/**
 * @file frame.c
 * @brief CRC-16 + COBS framing.
 */
#include "sn/frame.h"

#include <string.h>

#include "sn/crc.h"

size_t sn_frame_encode(const uint8_t *payload, size_t len, uint8_t *out, size_t cap)
{
    if ((len > SN_FRAME_MAX_PAYLOAD) || (out == NULL)) {
        return 0U;
    }
    uint8_t raw[SN_FRAME_MAX_PAYLOAD + SN_FRAME_CRC_SIZE];
    if (len > 0U) {
        (void)memcpy(raw, payload, len);
    }
    const uint16_t crc = sn_crc16(raw, len);
    raw[len] = (uint8_t)(crc & 0xFFU);
    raw[len + 1U] = (uint8_t)(crc >> 8);

    if (cap < 1U) {
        return 0U;
    }
    const size_t encoded = sn_cobs_encode(raw, len + SN_FRAME_CRC_SIZE, out, cap - 1U);
    if (encoded == 0U) {
        return 0U;
    }
    out[encoded] = 0U;
    return encoded + 1U;
}

void sn_frame_decoder_init(sn_frame_decoder_t *dec)
{
    dec->len = 0U;
    dec->overflow = false;
}

static sn_frame_status_t finish_block(const sn_frame_decoder_t *dec, uint8_t *payload, size_t cap,
                                      size_t *payload_len)
{
    uint8_t raw[SN_FRAME_MAX_PAYLOAD + SN_FRAME_CRC_SIZE];
    const size_t decoded = sn_cobs_decode(dec->block, dec->len, raw, sizeof(raw));
    if (decoded == 0U) {
        return SN_FRAME_ERR_COBS;
    }
    if (decoded <= SN_FRAME_CRC_SIZE) {
        return SN_FRAME_ERR_SHORT;
    }
    const size_t body = decoded - SN_FRAME_CRC_SIZE;
    const uint16_t expected = (uint16_t)((uint16_t)raw[body] | (uint16_t)((uint16_t)raw[body + 1U] << 8));
    if (sn_crc16(raw, body) != expected) {
        return SN_FRAME_ERR_CRC;
    }
    if (body > cap) {
        return SN_FRAME_ERR_OVERFLOW;
    }
    (void)memcpy(payload, raw, body);
    *payload_len = body;
    return SN_FRAME_OK;
}

sn_frame_status_t sn_frame_decoder_push(sn_frame_decoder_t *dec, uint8_t byte, uint8_t *payload,
                                        size_t cap, size_t *payload_len)
{
    if (byte != 0U) {
        if (dec->len < sizeof(dec->block)) {
            dec->block[dec->len] = byte;
            ++dec->len;
        } else {
            dec->overflow = true;
        }
        return SN_FRAME_NONE;
    }

    sn_frame_status_t status = SN_FRAME_NONE;
    if (dec->overflow) {
        status = SN_FRAME_ERR_OVERFLOW;
    } else if (dec->len > 0U) {
        status = finish_block(dec, payload, cap, payload_len);
    } else {
        status = SN_FRAME_NONE;
    }
    sn_frame_decoder_init(dec);
    return status;
}
