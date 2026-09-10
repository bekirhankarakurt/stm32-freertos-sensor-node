/**
 * @file flash_sim.h
 * @brief RAM-backed model of the STM32F401RE flash (sector geometry, 1->0 programming).
 *
 * Used by the Linux simulation and by host tests of the boot logic and
 * update protocol.
 */
#ifndef SN_FLASH_SIM_H
#define SN_FLASH_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "sn/flash_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SN_FLASH_SIM_SIZE 0x80000U /**< 512 KiB. */

/** Simulated flash device. */
typedef struct {
    uint8_t mem[SN_FLASH_SIM_SIZE]; /**< Contents (erased = 0xFF). */
    uint32_t erase_count;           /**< Sector erases performed. */
    uint32_t program_count;         /**< Program operations performed. */
    uint32_t fail_after;            /**< Fail the Nth next program (0 = never). */
} sn_flash_sim_t;

/** @brief Fill with 0xFF and reset counters. */
void sn_flash_sim_init(sn_flash_sim_t *sim);

/** @brief Driver interface bound to @p sim. */
sn_flash_if_t sn_flash_sim_if(sn_flash_sim_t *sim);

/** @brief Map a CPU address to the sector index (-1 when outside flash). */
int sn_flash_sector_of(uint32_t addr);

/** @brief Base address of sector @p index. */
uint32_t sn_flash_sector_base(int index);

/** @brief Size of sector @p index. */
uint32_t sn_flash_sector_size(int index);

#ifdef __cplusplus
}
#endif

#endif /* SN_FLASH_SIM_H */
