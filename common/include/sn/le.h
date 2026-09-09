/**
 * @file le.h
 * @brief Little-endian load/store helpers (alignment-agnostic).
 */
#ifndef SN_LE_H
#define SN_LE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Store a 16-bit value little-endian. */
void sn_le_put16(uint8_t *p, uint16_t v);

/** @brief Store a 32-bit value little-endian. */
void sn_le_put32(uint8_t *p, uint32_t v);

/** @brief Load a 16-bit little-endian value. */
uint16_t sn_le_get16(const uint8_t *p);

/** @brief Load a 32-bit little-endian value. */
uint32_t sn_le_get32(const uint8_t *p);

#ifdef __cplusplus
}
#endif

#endif /* SN_LE_H */
