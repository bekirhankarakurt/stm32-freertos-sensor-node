/**
 * @file sn_upload.cpp
 * @brief Firmware uploader: puts the node into its bootloader and writes the inactive slot.
 *
 * Example:
 *   sn_upload --port /dev/ttyACM0 --image-a sn_app_a.bin --image-b sn_app_b.bin
 */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "serial_port.hpp"
#include "sn/flash_layout.h"
#include "uploader.hpp"

namespace {

void usage()
{
    std::fprintf(stderr, "usage: sn_upload --port PATH --image-a FILE --image-b FILE [--baud N] [--no-boot]\n");
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

char slot_letter(std::uint8_t slot)
{
    if (slot == 0U) {
        return 'A';
    }
    return (slot == 1U) ? 'B' : '-';
}

std::string version_string(std::uint32_t ordinal)
{
    return std::to_string(ordinal >> 24) + "." + std::to_string((ordinal >> 16) & 0xFFU) + "." +
           std::to_string(ordinal & 0xFFFFU);
}

}  // namespace

int main(int argc, char** argv)
{
    std::string port;
    std::string image_path[2];
    unsigned baud = 115200U;
    bool do_boot = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const char* val = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if ((arg == "--port") && (val != nullptr)) {
            port = val;
            ++i;
        } else if ((arg == "--image-a") && (val != nullptr)) {
            image_path[0] = val;
            ++i;
        } else if ((arg == "--image-b") && (val != nullptr)) {
            image_path[1] = val;
            ++i;
        } else if ((arg == "--baud") && (val != nullptr)) {
            baud = static_cast<unsigned>(std::strtoul(val, nullptr, 10));
            ++i;
        } else if (arg == "--no-boot") {
            do_boot = false;
        } else {
            usage();
            return EXIT_FAILURE;
        }
    }
    if (port.empty() || image_path[0].empty() || image_path[1].empty()) {
        usage();
        return EXIT_FAILURE;
    }

    sn::tools::SerialPort link(port, "", baud);
    if (!link.ok() || !link.error().empty()) {
        std::fprintf(stderr, "sn_upload: %s\n", link.error().c_str());
        return EXIT_FAILURE;
    }
    sn::tools::Uploader up(link, 300, 5);

    std::printf("requesting bootloader...\n");
    (void)up.request_bootloader();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    sn::tools::BootloaderInfo info;
    if (!up.hello(info, 10000)) {
        std::fprintf(stderr, "sn_upload: %s\n", up.error().c_str());
        return EXIT_FAILURE;
    }
    std::printf("bootloader v%u.%u  active=%c  valid A:%s B:%s  target slot %c\n", info.bl_major, info.bl_minor,
                slot_letter(info.active),
                ((info.valid_mask & 1U) != 0U) ? version_string(info.version[0]).c_str() : "none",
                ((info.valid_mask & 2U) != 0U) ? version_string(info.version[1]).c_str() : "none",
                (info.target == 1U) ? 'B' : 'A');

    const unsigned target = (info.target == 1U) ? 1U : 0U;
    std::vector<std::uint8_t> image;
    if (!read_file(image_path[target], image)) {
        std::fprintf(stderr, "sn_upload: cannot read %s\n", image_path[target].c_str());
        return EXIT_FAILURE;
    }
    const std::uint32_t slot_addr = (target == 1U) ? SN_SLOT_B_ADDR : SN_SLOT_A_ADDR;
    const auto progress = [](std::size_t done, std::size_t total) {
        std::printf("\rwriting %6zu / %6zu bytes", done, total);
        std::fflush(stdout);
    };
    if (!up.upload(image, slot_addr, progress)) {
        std::fprintf(stderr, "\nsn_upload: %s\n", up.error().c_str());
        return EXIT_FAILURE;
    }
    std::printf("\nimage verified, slot %c marked for trial boot\n", (target == 1U) ? 'B' : 'A');
    if (do_boot) {
        if (!up.boot()) {
            std::fprintf(stderr, "sn_upload: %s\n", up.error().c_str());
            return EXIT_FAILURE;
        }
        std::printf("rebooting into the new image\n");
    }
    return EXIT_SUCCESS;
}
