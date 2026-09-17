/**
 * @file uploader.cpp
 * @brief Firmware update client.
 */
#include "uploader.hpp"

#include "sn/crc.h"
#include "sn/image.h"
#include "sn/le.h"
#include "sn/proto_ids.h"

namespace sn::tools {

Uploader::Uploader(Transport& link, int timeout_ms, int retries)
    : link_(link), timeout_ms_(timeout_ms), retries_(retries)
{
}

bool Uploader::transact(const std::vector<std::uint8_t>& req, std::uint8_t expect_type,
                        std::vector<std::uint8_t>& resp)
{
    for (int attempt = 0; attempt < retries_; ++attempt) {
        if (!link_.send(req)) {
            error_ = "transport send failed";
            return false;
        }
        while (link_.receive(resp, timeout_ms_)) {
            /* Ignore stale replies and stray application telemetry. */
            if ((resp.size() >= 2U) && (resp[0] == expect_type) && (resp[1] == req[1])) {
                return true;
            }
        }
    }
    error_ = "no reply from bootloader";
    return false;
}

bool Uploader::command(std::uint8_t type, const std::vector<std::uint8_t>& body, std::uint32_t& arg)
{
    std::vector<std::uint8_t> req{type, seq_++};
    req.insert(req.end(), body.begin(), body.end());
    std::vector<std::uint8_t> resp;
    if (!transact(req, SN_MSG_UPD_RESULT, resp)) {
        return false;
    }
    if ((resp.size() != 8U) || (resp[2] != type)) {
        error_ = "malformed result";
        return false;
    }
    arg = sn_le_get32(&resp[4]);
    if (resp[3] != SN_STATUS_OK) {
        error_ = "bootloader rejected command 0x" + std::to_string(type) + " with status " +
                 std::to_string(resp[3]) + " (arg " + std::to_string(arg) + ")";
        return false;
    }
    return true;
}

bool Uploader::request_bootloader()
{
    return link_.send({SN_MSG_CMD_BOOTLOADER, seq_++});
}

bool Uploader::hello(BootloaderInfo& info, int wait_ms)
{
    int waited = 0;
    while (waited <= wait_ms) {
        const std::vector<std::uint8_t> req{SN_MSG_UPD_HELLO, seq_++};
        if (!link_.send(req)) {
            error_ = "transport send failed";
            return false;
        }
        std::vector<std::uint8_t> resp;
        while (link_.receive(resp, timeout_ms_)) {
            if ((resp.size() == 20U) && (resp[0] == SN_MSG_UPD_INFO) && (resp[1] == req[1])) {
                info.protocol = resp[2];
                info.bl_major = resp[3];
                info.bl_minor = resp[4];
                info.active = resp[5];
                info.pending = resp[6];
                info.target = resp[7];
                info.valid_mask = resp[8];
                info.flags = resp[9];
                info.max_chunk = sn_le_get16(&resp[10]);
                info.version[0] = sn_le_get32(&resp[12]);
                info.version[1] = sn_le_get32(&resp[16]);
                if (info.protocol != SN_UPD_PROTOCOL_VERSION) {
                    error_ = "unsupported protocol version";
                    return false;
                }
                return true;
            }
        }
        waited += timeout_ms_;
    }
    error_ = "bootloader did not answer HELLO";
    return false;
}

bool Uploader::upload(const std::vector<std::uint8_t>& image, std::uint32_t slot_addr, const Progress& progress)
{
    std::uint32_t load_addr = 0U;
    if (!inspect_image(image, load_addr, error_)) {
        return false;
    }
    if (load_addr != slot_addr) {
        error_ = "image is linked for a different slot";
        return false;
    }
    std::vector<std::uint8_t> size(4U);
    sn_le_put32(size.data(), static_cast<std::uint32_t>(image.size()));
    std::uint32_t arg = 0U;
    if (!command(SN_MSG_UPD_BEGIN, size, arg)) {
        return false;
    }
    std::size_t offset = 0U;
    while (offset < image.size()) {
        const std::size_t n = std::min<std::size_t>(SN_UPD_MAX_CHUNK, image.size() - offset);
        std::vector<std::uint8_t> body(4U + n);
        sn_le_put32(body.data(), static_cast<std::uint32_t>(offset));
        std::copy(image.begin() + static_cast<std::ptrdiff_t>(offset),
                  image.begin() + static_cast<std::ptrdiff_t>(offset + n), body.begin() + 4);
        if (!command(SN_MSG_UPD_DATA, body, arg)) {
            return false;
        }
        offset += n;
        if (progress) {
            progress(offset, image.size());
        }
    }
    return command(SN_MSG_UPD_END, {}, arg);
}

bool Uploader::boot()
{
    std::uint32_t arg = 0U;
    return command(SN_MSG_UPD_BOOT, {}, arg);
}

bool inspect_image(const std::vector<std::uint8_t>& image, std::uint32_t& load_addr, std::string& error)
{
    if (image.size() <= SN_IMAGE_HEADER_AREA) {
        error = "image too small";
        return false;
    }
    if (sn_image_header_check(image.data()) != SN_IMAGE_OK) {
        error = "missing or corrupt image header (was the image signed by imgtool.py?)";
        return false;
    }
    sn_image_header_t hdr{};
    sn_image_header_decode(image.data(), &hdr);
    if ((hdr.image_size + SN_IMAGE_HEADER_AREA) != image.size()) {
        error = "header size does not match file size";
        return false;
    }
    if (sn_crc32(&image[SN_IMAGE_HEADER_AREA], hdr.image_size) != hdr.image_crc32) {
        error = "payload CRC mismatch";
        return false;
    }
    load_addr = hdr.load_addr;
    return true;
}

}  // namespace sn::tools
