/**
 * @file test_imgtool.cpp
 * @brief Cross-checks images signed by tools/imgtool.py against the C validator.
 *
 * Usage: test_imgtool <signed.bin> <expected-load-addr>
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

#include "uploader.hpp"

int main(int argc, char** argv)
{
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    std::ifstream in(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> img((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::uint32_t addr = 0U;
    std::string err;
    if (!sn::tools::inspect_image(img, addr, err)) {
        std::fprintf(stderr, "invalid image: %s\n", err.c_str());
        return EXIT_FAILURE;
    }
    const auto expected = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 0));
    std::printf("image ok, load address 0x%08lx\n", static_cast<unsigned long>(addr));
    return (addr == expected) ? EXIT_SUCCESS : EXIT_FAILURE;
}
