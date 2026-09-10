/**
 * @file flash_sim.c
 * @brief RAM-backed flash model with STM32F401 sector geometry.
 */
#include "sn/flash_sim.h"

#include <string.h>

#include "sn/flash_layout.h"

#define SECTOR_COUNT 8

static const uint32_t k_sector_size[SECTOR_COUNT] = {0x4000U,  0x4000U,  0x4000U,  0x4000U,
                                                     0x10000U, 0x20000U, 0x20000U, 0x20000U};

uint32_t sn_flash_sector_size(int index)
{
    return ((index >= 0) && (index < SECTOR_COUNT)) ? k_sector_size[index] : 0U;
}

uint32_t sn_flash_sector_base(int index)
{
    uint32_t base = SN_FLASH_BASE;
    for (int i = 0; (i < index) && (i < SECTOR_COUNT); ++i) {
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
    for (int i = 0; i < SECTOR_COUNT; ++i) {
        if (addr < (base + k_sector_size[i])) {
            return i;
        }
        base += k_sector_size[i];
    }
    return -1;
}

static bool in_range(uint32_t addr, size_t len)
{
    return (addr >= SN_FLASH_BASE) && (len <= SN_FLASH_SIM_SIZE) &&
           ((addr - SN_FLASH_BASE) <= (SN_FLASH_SIM_SIZE - len));
}

static bool sim_read(void *ctx, uint32_t addr, uint8_t *dst, size_t len)
{
    const sn_flash_sim_t *sim = (const sn_flash_sim_t *)ctx;
    if (!in_range(addr, len)) {
        return false;
    }
    (void)memcpy(dst, &sim->mem[addr - SN_FLASH_BASE], len);
    return true;
}

static bool sim_program(void *ctx, uint32_t addr, const uint8_t *src, size_t len)
{
    sn_flash_sim_t *sim = (sn_flash_sim_t *)ctx;
    if (!in_range(addr, len)) {
        return false;
    }
    if (sim->fail_after != 0U) {
        --sim->fail_after;
        if (sim->fail_after == 0U) {
            return false;
        }
    }
    uint8_t *dst = &sim->mem[addr - SN_FLASH_BASE];
    for (size_t i = 0U; i < len; ++i) {
        /* Programming can only clear bits. */
        if ((uint8_t)(dst[i] & src[i]) != src[i]) {
            return false;
        }
    }
    for (size_t i = 0U; i < len; ++i) {
        dst[i] = src[i];
    }
    ++sim->program_count;
    return true;
}

static bool sim_erase(void *ctx, uint32_t addr, uint32_t len)
{
    sn_flash_sim_t *sim = (sn_flash_sim_t *)ctx;
    if ((len == 0U) || !in_range(addr, len)) {
        return false;
    }
    const int first = sn_flash_sector_of(addr);
    const int last = sn_flash_sector_of(addr + len - 1U);
    for (int s = first; s <= last; ++s) {
        (void)memset(&sim->mem[sn_flash_sector_base(s) - SN_FLASH_BASE], 0xFF, sn_flash_sector_size(s));
        ++sim->erase_count;
    }
    return true;
}

void sn_flash_sim_init(sn_flash_sim_t *sim)
{
    (void)memset(sim->mem, 0xFF, sizeof(sim->mem));
    sim->erase_count = 0U;
    sim->program_count = 0U;
    sim->fail_after = 0U;
}

sn_flash_if_t sn_flash_sim_if(sn_flash_sim_t *sim)
{
    const sn_flash_if_t f = {sim, sim_read, sim_program, sim_erase};
    return f;
}
