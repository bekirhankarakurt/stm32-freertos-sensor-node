/**
 * @file image_fixture.hpp
 * @brief Builds synthetic, correctly signed slot images for tests.
 */
#ifndef SN_IMAGE_FIXTURE_HPP
#define SN_IMAGE_FIXTURE_HPP

#include <cstdint>
#include <vector>

#include "sn/crc.h"
#include "sn/flash_layout.h"
#include "sn/image.h"
#include "sn/le.h"

/** Create an image linked for @p slot_addr with @p payload_size bytes after the header area. */
inline std::vector<std::uint8_t> make_image(std::uint32_t slot_addr, std::uint32_t payload_size, std::uint8_t major,
                                            std::uint8_t minor, std::uint16_t patch)
{
    std::vector<std::uint8_t> img(SN_IMAGE_HEADER_AREA + payload_size, 0xFFU);
    for (std::uint32_t i = 8U; i < payload_size; ++i) {
        img[SN_IMAGE_HEADER_AREA + i] = static_cast<std::uint8_t>((i * 31U) ^ major);
    }
    sn_le_put32(&img[SN_IMAGE_HEADER_AREA], SN_RAM_BASE + SN_RAM_SIZE - SN_SHARED_RAM_SIZE);
    sn_le_put32(&img[SN_IMAGE_HEADER_AREA + 4U], slot_addr + SN_IMAGE_HEADER_AREA + 0x101U);
    sn_image_header_t hdr{};
    hdr.magic = SN_IMAGE_MAGIC;
    hdr.header_version = SN_IMAGE_HEADER_VERSION;
    hdr.header_area = SN_IMAGE_HEADER_AREA;
    hdr.load_addr = slot_addr;
    hdr.image_size = payload_size;
    hdr.image_crc32 = sn_crc32(&img[SN_IMAGE_HEADER_AREA], payload_size);
    hdr.ver_major = major;
    hdr.ver_minor = minor;
    hdr.ver_patch = patch;
    hdr.git_hash = 0xC0FFEE00U;
    sn_image_header_encode(&hdr, img.data());
    return img;
}

#endif  // SN_IMAGE_FIXTURE_HPP
