/**
 * @file image.c
 * @brief Image header codec and slot validation.
 */
#include "sn/image.h"

#include "sn/crc.h"
#include "sn/flash_layout.h"
#include "sn/le.h"

#define HEADER_CRC_SPAN (SN_IMAGE_HEADER_SIZE - 4U)
#define CRC_CHUNK 64U

void sn_image_header_decode(const uint8_t raw[SN_IMAGE_HEADER_SIZE], sn_image_header_t *hdr)
{
    hdr->magic = sn_le_get32(&raw[0]);
    hdr->header_version = sn_le_get16(&raw[4]);
    hdr->header_area = sn_le_get16(&raw[6]);
    hdr->load_addr = sn_le_get32(&raw[8]);
    hdr->image_size = sn_le_get32(&raw[12]);
    hdr->image_crc32 = sn_le_get32(&raw[16]);
    hdr->ver_major = raw[20];
    hdr->ver_minor = raw[21];
    hdr->ver_patch = sn_le_get16(&raw[22]);
    hdr->git_hash = sn_le_get32(&raw[24]);
    hdr->header_crc32 = sn_le_get32(&raw[28]);
}

void sn_image_header_encode(const sn_image_header_t *hdr, uint8_t raw[SN_IMAGE_HEADER_SIZE])
{
    sn_le_put32(&raw[0], hdr->magic);
    sn_le_put16(&raw[4], hdr->header_version);
    sn_le_put16(&raw[6], hdr->header_area);
    sn_le_put32(&raw[8], hdr->load_addr);
    sn_le_put32(&raw[12], hdr->image_size);
    sn_le_put32(&raw[16], hdr->image_crc32);
    raw[20] = hdr->ver_major;
    raw[21] = hdr->ver_minor;
    sn_le_put16(&raw[22], hdr->ver_patch);
    sn_le_put32(&raw[24], hdr->git_hash);
    sn_le_put32(&raw[28], sn_crc32(raw, HEADER_CRC_SPAN));
}

sn_image_status_t sn_image_header_check(const uint8_t raw[SN_IMAGE_HEADER_SIZE])
{
    sn_image_header_t hdr;
    sn_image_header_decode(raw, &hdr);
    if (hdr.magic != SN_IMAGE_MAGIC) {
        return SN_IMAGE_ERR_MAGIC;
    }
    if ((hdr.header_version != SN_IMAGE_HEADER_VERSION) || (hdr.header_area != SN_IMAGE_HEADER_AREA) ||
        (sn_crc32(raw, HEADER_CRC_SPAN) != hdr.header_crc32)) {
        return SN_IMAGE_ERR_HEADER;
    }
    return SN_IMAGE_OK;
}

static bool vectors_plausible(const sn_flash_if_t *flash, uint32_t slot_addr, uint32_t image_size)
{
    uint8_t vec[8];
    if (!flash->read(flash->ctx, slot_addr + SN_IMAGE_HEADER_AREA, vec, sizeof(vec))) {
        return false;
    }
    const uint32_t sp = sn_le_get32(&vec[0]);
    const uint32_t pc = sn_le_get32(&vec[4]);
    const uint32_t code_start = slot_addr + SN_IMAGE_HEADER_AREA;
    const uint32_t code_end = code_start + image_size;
    const bool sp_ok = (sp > SN_RAM_BASE) && (sp <= (SN_RAM_BASE + SN_RAM_SIZE)) && ((sp & 7U) == 0U);
    const bool pc_ok = ((pc & 1U) == 1U) && (pc > code_start) && (pc < code_end);
    return sp_ok && pc_ok;
}

sn_image_status_t sn_image_validate(const sn_flash_if_t *flash, uint32_t slot_addr,
                                    uint32_t slot_size, sn_image_header_t *hdr)
{
    uint8_t raw[SN_IMAGE_HEADER_SIZE];
    if (!flash->read(flash->ctx, slot_addr, raw, sizeof(raw))) {
        return SN_IMAGE_ERR_READ;
    }
    const sn_image_status_t hs = sn_image_header_check(raw);
    if (hs != SN_IMAGE_OK) {
        return hs;
    }
    sn_image_header_decode(raw, hdr);
    if (hdr->load_addr != slot_addr) {
        return SN_IMAGE_ERR_ADDRESS;
    }
    if ((hdr->image_size < 8U) || (hdr->image_size > (slot_size - SN_IMAGE_HEADER_AREA))) {
        return SN_IMAGE_ERR_SIZE;
    }

    uint32_t crc = SN_CRC32_INIT;
    uint8_t chunk[CRC_CHUNK];
    uint32_t done = 0U;
    while (done < hdr->image_size) {
        const uint32_t remaining = hdr->image_size - done;
        const uint32_t n = (remaining < CRC_CHUNK) ? remaining : CRC_CHUNK;
        if (!flash->read(flash->ctx, slot_addr + SN_IMAGE_HEADER_AREA + done, chunk, n)) {
            return SN_IMAGE_ERR_READ;
        }
        crc = sn_crc32_update(crc, chunk, n);
        done += n;
    }
    if (sn_crc32_final(crc) != hdr->image_crc32) {
        return SN_IMAGE_ERR_CRC;
    }
    if (!vectors_plausible(flash, slot_addr, hdr->image_size)) {
        return SN_IMAGE_ERR_VECTORS;
    }
    return SN_IMAGE_OK;
}

uint32_t sn_image_version_ordinal(const sn_image_header_t *hdr)
{
    return ((uint32_t)hdr->ver_major << 24) | ((uint32_t)hdr->ver_minor << 16) | (uint32_t)hdr->ver_patch;
}
