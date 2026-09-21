/**
 * @file flash_geometry.c
 * @brief STM32F401RE flash sector map.
 */
#include "sn/flash_geometry.h"

#include "sn/flash_layout.h"

static const uint32_t k_sector_size[SN_FLASH_SECTOR_COUNT] = {0x4000U,  0x4000U,  0x4000U,  0x4000U,
                                                              0x10000U, 0x20000U, 0x20000U, 0x20000U};

uint32_t sn_flash_sector_size(int index)
{
    return ((index >= 0) && (index < SN_FLASH_SECTOR_COUNT)) ? k_sector_size[index] : 0U;
}

uint32_t sn_flash_sector_base(int index)
{
    uint32_t base = SN_FLASH_BASE;
    for (int i = 0; (i < index) && (i < SN_FLASH_SECTOR_COUNT); ++i) {
        base += k_sector_size[i];
    }
    return base;
}

int sn_flash_sector_of(uint32_t addr)
{
    if (addr < SN_FLASH_BASE) {
        return -1;
    }
    uint32_t base = SN_FLASH_BASE;
    for (int i = 0; i < SN_FLASH_SECTOR_COUNT; ++i) {
        if (addr < (base + k_sector_size[i])) {
            return i;
        }
        base += k_sector_size[i];
    }
    return -1;
}
