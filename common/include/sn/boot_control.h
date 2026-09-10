/**
 * @file boot_control.h
 * @brief A/B slot selection with trial boots and automatic rollback.
 *
 * State lives in an append-only journal of 16-byte CRC-protected records in a
 * dedicated flash sector. Appending never needs an erase until the sector is
 * full, so a power cut can at worst lose the record being written; readers
 * always take the last valid record.
 */
#ifndef SN_BOOT_CONTROL_H
#define SN_BOOT_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "sn/flash_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SN_SLOT_A    0U    /**< Slot A index. */
#define SN_SLOT_B    1U    /**< Slot B index. */
#define SN_SLOT_NONE 0xFFU /**< No slot. */

#define SN_BOOT_MAX_ATTEMPTS 3U /**< Trial boots before rollback. */

#define SN_BOOT_FLAG_ROLLED_BACK 0x01U /**< Last trial image was rejected. */

#define SN_JOURNAL_RECORD_SIZE 16U          /**< Bytes per journal record. */
#define SN_JOURNAL_MAGIC       0x4C524E4AU /**< "JNRL". */

/** Persistent boot state. */
typedef struct {
    uint8_t active;   /**< Confirmed slot (::SN_SLOT_A/::SN_SLOT_B/::SN_SLOT_NONE). */
    uint8_t pending;  /**< Slot on trial, or ::SN_SLOT_NONE. */
    uint8_t attempts; /**< Trial boots consumed by @ref pending. */
    uint8_t flags;    /**< SN_BOOT_FLAG_* bits. */
} sn_boot_state_t;

/** Journal location and access. */
typedef struct {
    const sn_flash_if_t *flash; /**< Flash driver. */
    uint32_t base;              /**< Journal sector address. */
    uint32_t size;              /**< Journal sector size. */
} sn_journal_t;

/** Outcome of the boot decision. */
typedef struct {
    uint8_t boot_slot;     /**< Slot to start, or ::SN_SLOT_NONE (stay in bootloader). */
    sn_boot_state_t state; /**< State to persist before starting it. */
    bool persist;          /**< @ref state differs from the input. */
} sn_boot_decision_t;

/** @brief Default state for a blank journal. */
sn_boot_state_t sn_boot_state_default(void);

/**
 * @brief Load the latest valid state.
 * @param[out] state  Latest state, or the default when the journal is blank.
 * @param[out] seq    Sequence number of that record (0 when blank).
 * @return true when a valid record was found.
 */
bool sn_journal_load(const sn_journal_t *j, sn_boot_state_t *state, uint32_t *seq);

/**
 * @brief Append a new state record (erases and compacts when full).
 * @return true when the record was written and read back intact.
 */
bool sn_journal_store(const sn_journal_t *j, const sn_boot_state_t *state);

/**
 * @brief Pure boot decision.
 * @param state   Current persisted state.
 * @param valid_a Slot A holds a valid image.
 * @param valid_b Slot B holds a valid image.
 */
sn_boot_decision_t sn_boot_decide(const sn_boot_state_t *state, bool valid_a, bool valid_b);

/**
 * @brief Mark a freshly written slot as pending trial.
 * @return New state; @p slot is booted on the next reset.
 */
sn_boot_state_t sn_boot_mark_pending(const sn_boot_state_t *state, uint8_t slot);

/**
 * @brief Confirm the running slot after it proved healthy.
 * @param state   Current state.
 * @param running Slot the application runs from.
 * @param[out] changed true when the state needs persisting.
 */
sn_boot_state_t sn_boot_confirm(const sn_boot_state_t *state, uint8_t running, bool *changed);

/** @brief Slot an update should be written to (never the confirmed one). */
uint8_t sn_boot_update_target(const sn_boot_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* SN_BOOT_CONTROL_H */
