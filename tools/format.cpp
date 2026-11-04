/**
 * @file format.cpp
 * @brief Message rendering for sn_decode.
 */
#include "format.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "sn/le.h"
#include "sn/protocol.hpp"

namespace sn::tools {
namespace {

std::string printf_string(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

std::string printf_string(const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return (n > 0) ? std::string(buf) : std::string();
}

std::string fixed2(std::int64_t v)
{
    const char* sign = (v < 0) ? "-" : "";
    const std::int64_t a = (v < 0) ? -v : v;
    return printf_string("%s%lld.%02lld", sign, static_cast<long long>(a / 100), static_cast<long long>(a % 100));
}

std::string status_text(std::uint16_t status)
{
    std::string s;
    const auto add = [&s](const char* name) {
        if (!s.empty()) {
            s += ',';
        }
        s += name;
    };
    if ((status & proto::kStatusSensorFault) != 0U) {
        add("SENSOR_FAULT");
    }
    if ((status & proto::kStatusTrialBoot) != 0U) {
        add("TRIAL");
    }
    if ((status & proto::kStatusSlotB) != 0U) {
        add("SLOT_B");
    }
    if ((status & proto::kStatusRolledBack) != 0U) {
        add("ROLLED_BACK");
    }
    if ((status & proto::kStatusTxDrops) != 0U) {
        add("TX_DROPS");
    }
    return s.empty() ? std::string("ok") : s;
}

std::string version_text(const proto::Version& v)
{
    return printf_string("v%u.%u.%u slot=%c git=%08lx", v.major, v.minor, v.patch, (v.slot == 1U) ? 'B' : 'A',
                         static_cast<unsigned long>(v.git_hash));
}

std::string ack_text(const std::vector<std::uint8_t>& p)
{
    if (p.size() != proto::kAckSize) {
        return {};
    }
    static const char* const names[] = {"ok", "bad-length", "bad-state", "bad-offset", "flash-error",
                                        "verify-failed", "unknown-command", "out-of-range"};
    const char* status = (p[3] < (sizeof(names) / sizeof(names[0]))) ? names[p[3]] : "?";
    return printf_string("ACK  cmd=0x%02x status=%s", p[2], status);
}

}  // namespace

std::string format_text(const std::vector<std::uint8_t>& p)
{
    if (p.size() < 2U) {
        return {};
    }
    switch (p[0]) {
    case SN_MSG_TELEMETRY: {
        proto::Telemetry t{};
        if (!proto::decode(p.data(), p.size(), t)) {
            return {};
        }
        return printf_string("TELEM t=%8.3fs  T=%6s C  P=%9s hPa  RH=%6s %%  Tmin=%s Tmax=%s Tsd=%s  n=%lu  [%s]",
                             static_cast<double>(t.uptime_ms) / 1000.0, fixed2(t.temp_cdeg).c_str(),
                             fixed2(t.pressure_pa).c_str(), fixed2(t.humidity_cpct).c_str(),
                             fixed2(t.temp_min_cdeg).c_str(), fixed2(t.temp_max_cdeg).c_str(),
                             fixed2(t.temp_std_cdeg).c_str(), static_cast<unsigned long>(t.sample_count),
                             status_text(t.status).c_str());
    }
    case SN_MSG_HEALTH: {
        proto::Health h{};
        if (!proto::decode(p.data(), p.size(), h)) {
            return {};
        }
        const std::string cpu = (h.cpu_load_permille == proto::kCpuLoadUnavailable)
                                    ? std::string("n/a")
                                    : printf_string("%u.%u%%", h.cpu_load_permille / 10U, h.cpu_load_permille % 10U);
        return printf_string("HEALTH t=%7.3fs cpu=%s stack_min=%uw reset=%s rx_err=%u frame_err=%u tx_drop=%u "
                             "sensor_err=%u wdt_late=%u",
                             static_cast<double>(h.uptime_ms) / 1000.0, cpu.c_str(), h.min_stack_free,
                             proto::to_string(h.reset_cause), h.rx_errors, h.frame_errors, h.tx_drops,
                             h.sensor_errors, h.wdt_late);
    }
    case SN_MSG_BOOT: {
        proto::Boot b{};
        if (!proto::decode(p.data(), p.size(), b)) {
            return {};
        }
        return printf_string("BOOT  %s reset=%s trial_attempts=%u rolled_back=%s", version_text(b.version).c_str(),
                             proto::to_string(b.reset_cause), b.version.attempts,
                             ((b.boot_flags & 0x01U) != 0U) ? "yes" : "no");
    }
    case SN_MSG_VERSION: {
        proto::Version v{};
        if (!proto::decode_version(p.data(), p.size(), v)) {
            return {};
        }
        return "VERSION " + version_text(v);
    }
    case SN_MSG_ACK:
        return ack_text(p);
    default:
        return printf_string("MSG  type=0x%02x len=%zu", p[0], p.size());
    }
}

const char* csv_header()
{
    return "uptime_ms,temp_cdeg,pressure_pa,humidity_cpct,temp_min_cdeg,temp_max_cdeg,temp_std_cdeg,period_ms,"
           "status,sample_count";
}

std::string format_csv(const std::vector<std::uint8_t>& p)
{
    proto::Telemetry t{};
    if (!proto::decode(p.data(), p.size(), t)) {
        return {};
    }
    return printf_string("%lu,%d,%lu,%u,%d,%d,%u,%u,%u,%lu", static_cast<unsigned long>(t.uptime_ms), t.temp_cdeg,
                         static_cast<unsigned long>(t.pressure_pa), t.humidity_cpct, t.temp_min_cdeg,
                         t.temp_max_cdeg, t.temp_std_cdeg, t.period_ms, t.status,
                         static_cast<unsigned long>(t.sample_count));
}

bool parse_command(const std::string& text, std::uint8_t seq, std::vector<std::uint8_t>& payload)
{
    if (text == "version") {
        payload = {SN_MSG_CMD_GET_VERSION, seq};
        return true;
    }
    if (text == "reboot") {
        payload = {SN_MSG_CMD_REBOOT, seq};
        return true;
    }
    if (text == "bootloader") {
        payload = {SN_MSG_CMD_BOOTLOADER, seq};
        return true;
    }
    const std::string prefix = "period=";
    if (text.compare(0, prefix.size(), prefix) == 0) {
        const std::string num = text.substr(prefix.size());
        char* end = nullptr;
        const unsigned long v = std::strtoul(num.c_str(), &end, 10);
        if (num.empty() || (end == nullptr) || (*end != '\0') || (v > 0xFFFFUL)) {
            return false;
        }
        payload = {SN_MSG_CMD_SET_PERIOD, seq, 0U, 0U};
        sn_le_put16(&payload[2], static_cast<std::uint16_t>(v));
        return true;
    }
    return false;
}

}  // namespace sn::tools
