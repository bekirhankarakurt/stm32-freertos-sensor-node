/**
 * @file flash_if.h
 * @brief Abstract flash access used by portable boot logic (real driver or mock).
 */
#ifndef SN_FLASH_IF_H
#define SN_FLASH_IF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Flash operations; all addresses are absolute CPU addresses. */
typedef struct {
    void *ctx; /**< Implementation context. */
    /** Copy @p len bytes from flash into @p dst. */
    bool (*read)(void *ctx, uint32_t addr, uint8_t *dst, size_t len);
    /** Program bytes into erased flash (1 -> 0 transitions only). */
    bool (*program)(void *ctx, uint32_t addr, const uint8_t *src, size_t len);
    /** Erase every sector overlapping [addr, addr + len). */
    bool (*erase)(void *ctx, uint32_t addr, uint32_t len);
} sn_flash_if_t;

#ifdef __cplusplus
}
#endif

#endif /* SN_FLASH_IF_H */
