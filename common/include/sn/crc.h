/**
 * @file crc.h
 * @brief CRC-16/CCITT-FALSE (frame integrity) and CRC-32/ISO-HDLC (image integrity).
 */
#ifndef SN_CRC_H
#define SN_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initial value for CRC-16/CCITT-FALSE (poly 0x1021, no reflection, no xorout). */
#define SN_CRC16_INIT 0xFFFFU

/** Initial value for a running CRC-32 (poly 0x04C11DB7 reflected). */
#define SN_CRC32_INIT 0xFFFFFFFFU

/**
 * @brief Update a running CRC-16/CCITT-FALSE.
 * @param crc  Running value, start with ::SN_CRC16_INIT.
 * @param data Input bytes (may be NULL when @p len is 0).
 * @param len  Number of bytes.
 * @return Updated CRC; the final value needs no post-processing.
 */
uint16_t sn_crc16_update(uint16_t crc, const uint8_t *data, size_t len);

/** @brief One-shot CRC-16/CCITT-FALSE of a buffer (check value 0x29B1). */
uint16_t sn_crc16(const uint8_t *data, size_t len);

/**
 * @brief Update a running CRC-32 (unfinalised).
 * @param crc Running value, start with ::SN_CRC32_INIT.
 * @return Updated running value; apply sn_crc32_final() once at the end.
 */
uint32_t sn_crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/** @brief Finalise a running CRC-32 (bitwise inversion). */
uint32_t sn_crc32_final(uint32_t crc);

/** @brief One-shot CRC-32/ISO-HDLC of a buffer (check value 0xCBF43926). */
uint32_t sn_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SN_CRC_H */
