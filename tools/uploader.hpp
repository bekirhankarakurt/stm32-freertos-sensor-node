/**
 * @file uploader.hpp
 * @brief Host side of the firmware update protocol.
 */
#ifndef SN_TOOLS_UPLOADER_HPP
#define SN_TOOLS_UPLOADER_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "transport.hpp"

namespace sn::tools {

/** Bootloader status from the INFO reply. */
struct BootloaderInfo {
    std::uint8_t protocol{0U};   /**< Protocol revision. */
    std::uint8_t bl_major{0U};   /**< Bootloader version. */
    std::uint8_t bl_minor{0U};   /**< Bootloader version. */
    std::uint8_t active{0xFFU};  /**< Confirmed slot. */
    std::uint8_t pending{0xFFU}; /**< Slot on trial. */
    std::uint8_t target{0U};     /**< Slot the next update goes to. */
    std::uint8_t valid_mask{0U}; /**< Bit per valid slot. */
    std::uint8_t flags{0U};      /**< Journal flags. */
    std::uint16_t max_chunk{0U}; /**< Largest DATA chunk. */
    std::uint32_t version[2]{};  /**< Version ordinal per slot. */
};

/** Stop-and-wait uploader with per-request retries. */
class Uploader {
public:
    /** Progress callback: bytes sent, total. */
    using Progress = std::function<void(std::size_t, std::size_t)>;

    /**
     * @param link       Framed transport.
     * @param timeout_ms Reply timeout per attempt.
     * @param retries    Attempts per request.
     */
    Uploader(Transport& link, int timeout_ms, int retries);

    /** Ask a running application to reset into the bootloader (no reply expected). */
    bool request_bootloader();
    /** Discover the bootloader (repeats HELLO until @p wait_ms elapses). */
    bool hello(BootloaderInfo& info, int wait_ms);
    /**
     * Upload an image.
     * @param image  Complete slot image (header + payload) linked for @p slot_addr.
     * @param slot_addr Expected load address (checked against the image header).
     */
    bool upload(const std::vector<std::uint8_t>& image, std::uint32_t slot_addr, const Progress& progress);
    /** Leave the bootloader. */
    bool boot();

    /** Description of the last failure. */
    const std::string& error() const { return error_; }

private:
    bool transact(const std::vector<std::uint8_t>& req, std::uint8_t expect_type, std::vector<std::uint8_t>& resp);
    bool command(std::uint8_t type, const std::vector<std::uint8_t>& body, std::uint32_t& arg);

    Transport& link_;
    int timeout_ms_;
    int retries_;
    std::uint8_t seq_{0U};
    std::string error_;
};

/** Validate an image file locally and return its link address. */
bool inspect_image(const std::vector<std::uint8_t>& image, std::uint32_t& load_addr, std::string& error);

}  // namespace sn::tools

#endif  // SN_TOOLS_UPLOADER_HPP
