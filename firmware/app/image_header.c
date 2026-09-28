/**
 * @file image_header.c
 * @brief Header template placed at the start of the slot; size and CRCs are filled in after linking.
 */
#include "sn/image.h"

__attribute__((section(".image_header"), used)) const sn_image_header_t g_image_header = {
    SN_IMAGE_MAGIC,
    SN_IMAGE_HEADER_VERSION,
    (uint16_t)SN_IMAGE_HEADER_AREA,
    SN_SLOT_ADDR,
    0U,
    0U,
    (uint8_t)SN_VERSION_MAJOR,
    (uint8_t)SN_VERSION_MINOR,
    (uint16_t)SN_VERSION_PATCH,
    (uint32_t)SN_GIT_HASH,
    0U,
};
