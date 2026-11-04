/**
 * @file sn_decode.cpp
 * @brief Decode the node's binary telemetry stream and send commands.
 *
 * Examples:
 *   sn_decode --port /dev/ttyACM0
 *   sn_decode --port /dev/ttyACM0 --send period=50 --send version --count 20
 *   sn_sim --duration-ms 3000 | sn_decode --port -
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "format.hpp"
#include "serial_port.hpp"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: sn_decode --port PATH|- [--tx PATH] [--baud N] [--csv] [--count N] [--timeout-ms N]\n"
                 "                 [--send version|period=MS|reboot|bootloader]...\n");
}

bool parse_long(const char* text, long& out)
{
    char* end = nullptr;
    out = std::strtol(text, &end, 10);
    return (end != text) && (*end == '\0') && (out >= 0);
}

}  // namespace

int main(int argc, char** argv)
{
    std::string port;
    std::string tx;
    long baud = 115200;
    long count = 0;
    long timeout_ms = 5000;
    bool csv = false;
    std::vector<std::string> sends;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--csv") {
            csv = true;
            continue;
        }
        /* All other options take one value. */
        const char* val = (i + 1 < argc) ? argv[i + 1] : nullptr;
        ++i;
        bool ok = (val != nullptr);
        if (!ok) {
            /* Missing value: reported below. */
        } else if (arg == "--port") {
            port = val;
        } else if (arg == "--tx") {
            tx = val;
        } else if (arg == "--baud") {
            ok = parse_long(val, baud);
        } else if (arg == "--count") {
            ok = parse_long(val, count);
        } else if (arg == "--timeout-ms") {
            ok = parse_long(val, timeout_ms);
        } else if (arg == "--send") {
            sends.emplace_back(val);
        } else {
            ok = false;
        }
        if (!ok) {
            usage();
            return EXIT_FAILURE;
        }
    }
    if (port.empty()) {
        usage();
        return EXIT_FAILURE;
    }

    sn::tools::SerialPort link(port, tx, static_cast<unsigned>(baud));
    if (!link.ok() || !link.error().empty()) {
        std::fprintf(stderr, "sn_decode: %s\n", link.error().c_str());
        return EXIT_FAILURE;
    }

    std::uint8_t seq = 0x80U;
    for (const std::string& cmd : sends) {
        std::vector<std::uint8_t> payload;
        if (!sn::tools::parse_command(cmd, seq, payload)) {
            std::fprintf(stderr, "sn_decode: unknown command '%s'\n", cmd.c_str());
            return EXIT_FAILURE;
        }
        ++seq;
        if (!link.send(payload)) {
            std::fprintf(stderr, "sn_decode: cannot send '%s' (no TX path)\n", cmd.c_str());
            return EXIT_FAILURE;
        }
    }

    if (csv) {
        std::printf("%s\n", sn::tools::csv_header());
    }
    long printed = 0;
    std::vector<std::uint8_t> payload;
    while ((count == 0) || (printed < count)) {
        if (!link.receive(payload, static_cast<int>(timeout_ms))) {
            break;
        }
        const std::string line = csv ? sn::tools::format_csv(payload) : sn::tools::format_text(payload);
        if (!line.empty()) {
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
            ++printed;
        }
    }
    if (link.bad_frames() > 0U) {
        std::fprintf(stderr, "sn_decode: %u corrupt frame(s) skipped\n", link.bad_frames());
    }
    return (printed > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
