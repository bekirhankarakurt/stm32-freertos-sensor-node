/**
 * @file image.h
 * @brief Application image header and validation.
 *
 * Every slot starts with a 512-byte header area; the vector table follows at
 * offset ::SN_IMAGE_HEADER_AREA. The build patches size and CRCs after linking
 * (tools/imgtool.py).
 */
#ifndef SN_IMAGE_H
#define SN_IMAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "sn/flash_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SN_IMAGE_MAGIC          0x4D494E53U /**< "SNIM" little-endian. */
#define SN_IMAGE_HEADER_VERSION 1U           /**< Layout revision. */
#define SN_IMAGE_HEADER_AREA    0x200U      /**< Reserved bytes before the vector table. */
#define SN_IMAGE_HEADER_SIZE    32U          /**< Bytes covered by the header struct. */

/** On-flash image header (little-endian, packed by construction). */
typedef struct {
    uint32_t magic;          /**< ::SN_IMAGE_MAGIC. */
    uint16_t header_version; /**< ::SN_IMAGE_HEADER_VERSION. */
    uint16_t header_area;    /**< ::SN_IMAGE_HEADER_AREA. */
    uint32_t load_addr;      /**< Slot base the image was linked for. */
    uint32_t image_size;     /**< Bytes following the header area. */
    uint32_t image_crc32;    /**< CRC-32 over those bytes. */
    uint8_t ver_major;       /**< Semantic version major. */
    uint8_t ver_minor;       /**< Semantic version minor. */
    uint16_t ver_patch;      /**< Semantic version patch. */
    uint32_t git_hash;       /**< First 8 hex digits of the source revision. */
    uint32_t header_crc32;   /**< CRC-32 over the preceding 28 bytes. */
} sn_image_header_t;

/** Validation result. */
typedef enum {
    SN_IMAGE_OK = 0,        /**< Header and payload are valid. */
    SN_IMAGE_ERR_READ,      /**< Flash read failed. */
    SN_IMAGE_ERR_MAGIC,     /**< Erased or foreign content. */
    SN_IMAGE_ERR_HEADER,    /**< Header CRC/version/area mismatch. */
    SN_IMAGE_ERR_ADDRESS,   /**< Linked for a different slot. */
    SN_IMAGE_ERR_SIZE,      /**< Payload does not fit the slot. */
    SN_IMAGE_ERR_CRC,       /**< Payload CRC mismatch. */
    SN_IMAGE_ERR_VECTORS    /**< Initial SP/PC implausible. */
} sn_image_status_t;

/** @brief Decode a header from 32 little-endian bytes. */
void sn_image_header_decode(const uint8_t raw[SN_IMAGE_HEADER_SIZE], sn_image_header_t *hdr);

/** @brief Encode a header into 32 little-endian bytes (header_crc32 recomputed). */
void sn_image_header_encode(const sn_image_header_t *hdr, uint8_t raw[SN_IMAGE_HEADER_SIZE]);

/** @brief Check magic, version, area and header CRC of decoded bytes. */
sn_image_status_t sn_image_header_check(const uint8_t raw[SN_IMAGE_HEADER_SIZE]);

/**
 * @brief Fully validate the image stored at @p slot_addr.
 * @param flash     Flash access.
 * @param slot_addr Slot base address.
 * @param slot_size Slot capacity in bytes.
 * @param[out] hdr  Decoded header (valid when ::SN_IMAGE_OK is returned).
 */
sn_image_status_t sn_image_validate(const sn_flash_if_t *flash, uint32_t slot_addr,
                                    uint32_t slot_size, sn_image_header_t *hdr);

/** @brief Pack a version as 0x00MMmmpp-style ordinal for comparisons. */
uint32_t sn_image_version_ordinal(const sn_image_header_t *hdr);

#ifdef __cplusplus
}
#endif

#endif /* SN_IMAGE_H */
