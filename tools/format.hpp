/**
 * @file format.hpp
 * @brief Human-readable and CSV rendering of node messages.
 */
#ifndef SN_TOOLS_FORMAT_HPP
#define SN_TOOLS_FORMAT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sn::tools {

/** Render a payload as one log line (empty for unknown types). */
std::string format_text(const std::vector<std::uint8_t>& payload);

/** Render a TELEMETRY payload as CSV (empty for other types). */
std::string format_csv(const std::vector<std::uint8_t>& payload);

/** CSV header matching format_csv(). */
const char* csv_header();

/**
 * Parse a command such as "version", "period=50", "reboot" or "bootloader".
 * @return false when the text is not a known command.
 */
bool parse_command(const std::string& text, std::uint8_t seq, std::vector<std::uint8_t>& payload);

}  // namespace sn::tools

#endif  // SN_TOOLS_FORMAT_HPP
