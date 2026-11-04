/**
 * @file cobs.c
 * @brief COBS encoder/decoder.
 */
#include "sn/cobs.h"

size_t sn_cobs_encode(const uint8_t *src, size_t len, uint8_t *dst, size_t cap)
{
    if ((dst == NULL) || (cap < SN_COBS_MAX_ENCODED(len))) {
        return 0U;
    }
    size_t out = 1U;
    size_t code_pos = 0U;
    uint8_t code = 1U;

    for (size_t i = 0U; i < len; ++i) {
        if (src[i] == 0U) {
            dst[code_pos] = code;
            code_pos = out;
            ++out;
            code = 1U;
        } else {
            dst[out] = src[i];
            ++out;
            ++code;
            if (code == 0xFFU) {
                dst[code_pos] = code;
                code_pos = out;
                ++out;
                code = 1U;
            }
        }
    }
    dst[code_pos] = code;
    return out;
}

size_t sn_cobs_decode(const uint8_t *src, size_t len, uint8_t *dst, size_t cap)
{
    if ((src == NULL) || (dst == NULL) || (len == 0U)) {
        return 0U;
    }
    size_t in = 0U;
    size_t out = 0U;

    while (in < len) {
        const uint8_t code = src[in];
        if (code == 0U) {
            return 0U;
        }
        ++in;
        const size_t run = (size_t)code - 1U;
        if (((in + run) > len) || ((out + run) > cap)) {
            return 0U;
        }
        for (size_t k = 0U; k < run; ++k) {
            if (src[in] == 0U) {
                return 0U;
            }
            dst[out] = src[in];
            ++out;
            ++in;
        }
        if ((code != 0xFFU) && (in < len)) {
            if (out >= cap) {
                return 0U;
            }
            dst[out] = 0U;
            ++out;
        }
    }
    return out;
}
