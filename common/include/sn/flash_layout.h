/**
 * @file flash_layout.h
 * @brief STM32F401RE (512 KiB) flash partitioning shared by bootloader, app and tools.
 *
 * | Region      | Sector(s) | Address    | Size    |
 * |-------------|-----------|------------|---------|
 * | Bootloader  | 0         | 0x08000000 | 16 KiB  |
 * | Boot journal| 1         | 0x08004000 | 16 KiB  |
 * | Reserved    | 2..4      | 0x08008000 | 96 KiB  |
 * | Slot A      | 5         | 0x08020000 | 128 KiB |
 * | Slot B      | 6         | 0x08040000 | 128 KiB |
 * | Reserved    | 7         | 0x08060000 | 128 KiB |
 */
#ifndef SN_FLASH_LAYOUT_H
#define SN_FLASH_LAYOUT_H

#define SN_FLASH_BASE        0x08000000U /**< Start of on-chip flash. */
#define SN_BOOTLOADER_ADDR   0x08000000U /**< Bootloader (sector 0). */
#define SN_BOOTLOADER_SIZE   0x00004000U /**< 16 KiB. */
#define SN_JOURNAL_ADDR      0x08004000U /**< Boot-control journal (sector 1). */
#define SN_JOURNAL_SIZE      0x00004000U /**< 16 KiB. */
#define SN_SLOT_A_ADDR       0x08020000U /**< Application slot A (sector 5). */
#define SN_SLOT_B_ADDR       0x08040000U /**< Application slot B (sector 6). */
#define SN_SLOT_SIZE         0x00020000U /**< 128 KiB per slot. */

#define SN_RAM_BASE          0x20000000U /**< Start of SRAM. */
#define SN_RAM_SIZE          0x00018000U /**< 96 KiB. */
#define SN_SHARED_RAM_SIZE   32U         /**< Bootloader/app mailbox at the end of SRAM. */
#define SN_SHARED_RAM_ADDR   (SN_RAM_BASE + SN_RAM_SIZE - SN_SHARED_RAM_SIZE)

/** Mailbox value asking the bootloader to stay in update mode. */
#define SN_MAILBOX_ENTER_UPDATE 0x55504454U /* "UPDT" */

#endif /* SN_FLASH_LAYOUT_H */
