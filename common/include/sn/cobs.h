/**
 * @file cobs.h
 * @brief Consistent Overhead Byte Stuffing (COBS) encoder/decoder.
 *
 * COBS removes every 0x00 byte from a block so that 0x00 can be used as an
 * unambiguous frame delimiter on a byte stream (UART).
 */
#ifndef SN_COBS_H
#define SN_COBS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Worst-case encoded size of @p n input bytes (without the delimiter). */
#define SN_COBS_MAX_ENCODED(n) ((n) + ((n) / 254U) + 1U)

/**
 * @brief Encode a block.
 * @param src Input bytes.
 * @param len Number of input bytes.
 * @param dst Output buffer (must not overlap @p src).
 * @param cap Output capacity.
 * @return Encoded length, or 0 if @p cap is too small.
 */
size_t sn_cobs_encode(const uint8_t *src, size_t len, uint8_t *dst, size_t cap);

/**
 * @brief Decode a block (delimiter already stripped).
 * @return Decoded length, or 0 on malformed input or insufficient @p cap.
 */
size_t sn_cobs_decode(const uint8_t *src, size_t len, uint8_t *dst, size_t cap);

#ifdef __cplusplus
}
#endif

#endif /* SN_COBS_H */
