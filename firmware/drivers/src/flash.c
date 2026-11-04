/**
 * @file flash.c
 * @brief Internal flash erase/program (single bank, 2.7-3.6 V parallelism).
 *
 * The CPU stalls on instruction fetch while an operation is in progress, so
 * the driver can run from flash; the data cache is reset after erases so
 * stale lines are never returned.
 */
#include <string.h>

#include "sn/drv.h"
#include "sn/flash_geometry.h"
#include "sn/flash_layout.h"
#include "stm32f4xx.h"

#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU
#define FLASH_ERRORS (FLASH_SR_PGSERR | FLASH_SR_PGPERR | FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_RDERR)
#define SN_FLASH_LIMIT (SN_FLASH_BASE + 0x80000U)

static bool wait_idle(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    }
    const bool ok = (FLASH->SR & FLASH_ERRORS) == 0U;
    FLASH->SR = FLASH_ERRORS | FLASH_SR_EOP;
    return ok;
}

static void unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    FLASH->SR = FLASH_ERRORS | FLASH_SR_EOP;
}

static void lock(void)
{
    FLASH->CR = FLASH_CR_LOCK;
}

static bool in_flash(uint32_t addr, size_t len)
{
    return (addr >= SN_FLASH_BASE) && (addr < SN_FLASH_LIMIT) && (len <= (SN_FLASH_LIMIT - addr));
}

static bool drv_read(void *ctx, uint32_t addr, uint8_t *dst, size_t len)
{
    (void)ctx;
    if (!in_flash(addr, len)) {
        return false;
    }
    (void)memcpy(dst, (const void *)addr, len);
    return true;
}

static bool drv_program(void *ctx, uint32_t addr, const uint8_t *src, size_t len)
{
    (void)ctx;
    if (!in_flash(addr, len)) {
        return false;
    }
    unlock();
    bool ok = true;
    size_t i = 0U;
    while ((i < len) && ok) {
        const uint32_t a = addr + (uint32_t)i;
        if (((a & 3U) == 0U) && ((len - i) >= 4U)) {
            uint32_t word;
            (void)memcpy(&word, &src[i], sizeof(word));
            FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
            *(volatile uint32_t *)a = word;
            __DSB();
            i += 4U;
        } else {
            FLASH->CR = FLASH_CR_PG;
            *(volatile uint8_t *)a = src[i];
            __DSB();
            ++i;
        }
        ok = wait_idle();
    }
    lock();
    return ok;
}

static bool drv_erase(void *ctx, uint32_t addr, uint32_t len)
{
    (void)ctx;
    if ((len == 0U) || !in_flash(addr, len)) {
        return false;
    }
    const int first = sn_flash_sector_of(addr);
    const int last = sn_flash_sector_of(addr + len - 1U);
    unlock();
    bool ok = true;
    for (int s = first; (s <= last) && ok; ++s) {
        FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_SER | ((uint32_t)s << FLASH_CR_SNB_Pos);
        FLASH->CR |= FLASH_CR_STRT;
        ok = wait_idle();
    }
    lock();
    const uint32_t acr = FLASH->ACR;
    FLASH->ACR = acr & ~FLASH_ACR_DCEN;
    FLASH->ACR |= FLASH_ACR_DCRST;
    FLASH->ACR &= ~FLASH_ACR_DCRST;
    FLASH->ACR = acr;
    return ok;
}

static const sn_flash_if_t g_flash_if = {NULL, drv_read, drv_program, drv_erase};

const sn_flash_if_t *sn_flash_driver(void)
{
    return &g_flash_if;
}
