/**
 * @file proto_ids.h
 * @brief Message type identifiers shared by firmware, bootloader and host tools.
 *
 * Every payload starts with `type (u8)` and `seq (u8)`; all multi-byte
 * fields are little-endian.
 */
#ifndef SN_PROTO_IDS_H
#define SN_PROTO_IDS_H

/* Node -> host (application). */
#define SN_MSG_TELEMETRY        0x01U /**< Filtered sample + statistics. */
#define SN_MSG_HEALTH           0x02U /**< Periodic health report. */
#define SN_MSG_BOOT             0x03U /**< Emitted once after start-up. */
#define SN_MSG_VERSION          0x20U /**< Reply to ::SN_MSG_CMD_GET_VERSION. */
#define SN_MSG_ACK              0x21U /**< Generic command acknowledgement. */

/* Host -> node (application). */
#define SN_MSG_CMD_GET_VERSION  0x10U /**< Query firmware version. */
#define SN_MSG_CMD_SET_PERIOD   0x11U /**< Set sampling period (u16 ms). */
#define SN_MSG_CMD_REBOOT       0x12U /**< Software reset. */
#define SN_MSG_CMD_BOOTLOADER   0x13U /**< Reset into bootloader update mode. */

/* Host -> bootloader. */
#define SN_MSG_UPD_HELLO        0x40U /**< Discover bootloader, returns ::SN_MSG_UPD_INFO. */
#define SN_MSG_UPD_BEGIN        0x41U /**< Erase target slot (u32 total size). */
#define SN_MSG_UPD_DATA         0x42U /**< Program chunk (u32 offset + bytes). */
#define SN_MSG_UPD_END          0x43U /**< Verify image and mark it pending. */
#define SN_MSG_UPD_BOOT         0x44U /**< Leave the bootloader. */

/* Bootloader -> host. */
#define SN_MSG_UPD_INFO         0x50U /**< Bootloader and slot status. */
#define SN_MSG_UPD_RESULT       0x51U /**< Result of an update command. */

/* Command / update status codes. */
#define SN_STATUS_OK            0x00U /**< Success. */
#define SN_STATUS_BAD_LENGTH    0x01U /**< Malformed payload. */
#define SN_STATUS_BAD_STATE     0x02U /**< Command not valid now. */
#define SN_STATUS_BAD_OFFSET    0x03U /**< Offset/size outside the slot. */
#define SN_STATUS_FLASH_ERROR   0x04U /**< Erase/program failed. */
#define SN_STATUS_VERIFY_FAILED 0x05U /**< Image validation failed (arg = reason). */
#define SN_STATUS_UNKNOWN       0x06U /**< Unknown message type. */
#define SN_STATUS_OUT_OF_RANGE  0x07U /**< Parameter outside allowed range. */

#define SN_UPD_PROTOCOL_VERSION 1U   /**< Update protocol revision. */
#define SN_UPD_MAX_CHUNK        256U /**< Largest DATA chunk. */

#endif /* SN_PROTO_IDS_H */
