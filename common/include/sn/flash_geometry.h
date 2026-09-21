/**
 * @file flash_geometry.h
 * @brief STM32F401RE flash sector map (4 x 16 KiB, 1 x 64 KiB, 3 x 128 KiB).
 */
#ifndef SN_FLASH_GEOMETRY_H
#define SN_FLASH_GEOMETRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SN_FLASH_SECTOR_COUNT 8 /**< Sectors on the 512 KiB part. */

/** @brief Map a CPU address to the sector index (-1 when outside flash). */
int sn_flash_sector_of(uint32_t addr);

/** @brief Base address of sector @p index. */
uint32_t sn_flash_sector_base(int index);

/** @brief Size of sector @p index (0 for an invalid index). */
uint32_t sn_flash_sector_size(int index);

#ifdef __cplusplus
}
#endif

#endif /* SN_FLASH_GEOMETRY_H */
