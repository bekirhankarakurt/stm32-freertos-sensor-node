/**
 * @file protocol.cpp
 * @brief Application message codecs.
 */
#include "sn/protocol.hpp"

#include "sn/le.h"

namespace sn::proto {
namespace {

void put_header(std::uint8_t* out, std::uint8_t type, std::uint8_t seq)
{
    out[0] = type;
    out[1] = seq;
}

void put_version(std::uint8_t* out, const Version& v)
{
    out[0] = v.major;
    out[1] = v.minor;
    sn_le_put16(&out[2], v.patch);
    out[4] = v.slot;
    out[5] = v.attempts;
    sn_le_put32(&out[6], v.git_hash);
}

void get_version(const std::uint8_t* in, Version& v)
{
    v.major = in[0];
    v.minor = in[1];
    v.patch = sn_le_get16(&in[2]);
    v.slot = in[4];
    v.attempts = in[5];
    v.git_hash = sn_le_get32(&in[6]);
}

std::int16_t get_i16(const std::uint8_t* p)
{
    return static_cast<std::int16_t>(sn_le_get16(p));
}

void put_i16(std::uint8_t* p, std::int16_t v)
{
    sn_le_put16(p, static_cast<std::uint16_t>(v));
}

ResetCause to_cause(std::uint8_t raw)
{
    return (raw <= static_cast<std::uint8_t>(ResetCause::kBrownout)) ? static_cast<ResetCause>(raw)
                                                                       : ResetCause::kUnknown;
}

}  // namespace

std::size_t encode(const Telemetry& t, std::uint8_t seq, std::uint8_t* out, std::size_t cap)
{
    if (cap < kTelemetrySize) {
        return 0U;
    }
    put_header(out, SN_MSG_TELEMETRY, seq);
    sn_le_put32(&out[2], t.uptime_ms);
    put_i16(&out[6], t.temp_cdeg);
    sn_le_put32(&out[8], t.pressure_pa);
    sn_le_put16(&out[12], t.humidity_cpct);
    put_i16(&out[14], t.temp_min_cdeg);
    put_i16(&out[16], t.temp_max_cdeg);
    sn_le_put16(&out[18], t.temp_std_cdeg);
    sn_le_put16(&out[20], t.period_ms);
    sn_le_put16(&out[22], t.status);
    sn_le_put32(&out[24], t.sample_count);
    return kTelemetrySize;
}

std::size_t encode(const Health& h, std::uint8_t seq, std::uint8_t* out, std::size_t cap)
{
    if (cap < kHealthSize) {
        return 0U;
    }
    put_header(out, SN_MSG_HEALTH, seq);
    sn_le_put32(&out[2], h.uptime_ms);
    sn_le_put16(&out[6], h.cpu_load_permille);
    sn_le_put16(&out[8], h.min_stack_free);
    out[10] = static_cast<std::uint8_t>(h.reset_cause);
    out[11] = h.boot_flags;
    sn_le_put16(&out[12], h.rx_errors);
    sn_le_put16(&out[14], h.frame_errors);
    sn_le_put16(&out[16], h.tx_drops);
    sn_le_put16(&out[18], h.sensor_errors);
    sn_le_put16(&out[20], h.wdt_late);
    return kHealthSize;
}

std::size_t encode(const Boot& b, std::uint8_t seq, std::uint8_t* out, std::size_t cap)
{
    if (cap < kBootSize) {
        return 0U;
    }
    put_header(out, SN_MSG_BOOT, seq);
    put_version(&out[2], b.version);
    out[12] = static_cast<std::uint8_t>(b.reset_cause);
    out[13] = b.boot_flags;
    return kBootSize;
}

std::size_t encode_version(const Version& v, std::uint8_t seq, std::uint8_t* out, std::size_t cap)
{
    if (cap < kVersionSize) {
        return 0U;
    }
    put_header(out, SN_MSG_VERSION, seq);
    put_version(&out[2], v);
    return kVersionSize;
}

std::size_t encode_ack(std::uint8_t cmd, std::uint8_t status, std::uint8_t seq, std::uint8_t* out,
                       std::size_t cap)
{
    if (cap < kAckSize) {
        return 0U;
    }
    put_header(out, SN_MSG_ACK, seq);
    out[2] = cmd;
    out[3] = status;
    return kAckSize;
}

bool decode(const std::uint8_t* in, std::size_t len, Telemetry& t)
{
    if ((len != kTelemetrySize) || (in[0] != SN_MSG_TELEMETRY)) {
        return false;
    }
    t.uptime_ms = sn_le_get32(&in[2]);
    t.temp_cdeg = get_i16(&in[6]);
    t.pressure_pa = sn_le_get32(&in[8]);
    t.humidity_cpct = sn_le_get16(&in[12]);
    t.temp_min_cdeg = get_i16(&in[14]);
    t.temp_max_cdeg = get_i16(&in[16]);
    t.temp_std_cdeg = sn_le_get16(&in[18]);
    t.period_ms = sn_le_get16(&in[20]);
    t.status = sn_le_get16(&in[22]);
    t.sample_count = sn_le_get32(&in[24]);
    return true;
}

bool decode(const std::uint8_t* in, std::size_t len, Health& h)
{
    if ((len != kHealthSize) || (in[0] != SN_MSG_HEALTH)) {
        return false;
    }
    h.uptime_ms = sn_le_get32(&in[2]);
    h.cpu_load_permille = sn_le_get16(&in[6]);
    h.min_stack_free = sn_le_get16(&in[8]);
    h.reset_cause = to_cause(in[10]);
    h.boot_flags = in[11];
    h.rx_errors = sn_le_get16(&in[12]);
    h.frame_errors = sn_le_get16(&in[14]);
    h.tx_drops = sn_le_get16(&in[16]);
    h.sensor_errors = sn_le_get16(&in[18]);
    h.wdt_late = sn_le_get16(&in[20]);
    return true;
}

bool decode(const std::uint8_t* in, std::size_t len, Boot& b)
{
    if ((len != kBootSize) || (in[0] != SN_MSG_BOOT)) {
        return false;
    }
    get_version(&in[2], b.version);
    b.reset_cause = to_cause(in[12]);
    b.boot_flags = in[13];
    return true;
}

bool decode_version(const std::uint8_t* in, std::size_t len, Version& v)
{
    if ((len != kVersionSize) || (in[0] != SN_MSG_VERSION)) {
        return false;
    }
    get_version(&in[2], v);
    return true;
}

const char* to_string(ResetCause cause)
{
    switch (cause) {
    case ResetCause::kPowerOn:
        return "power-on";
    case ResetCause::kPin:
        return "pin";
    case ResetCause::kSoftware:
        return "software";
    case ResetCause::kIndependentWatchdog:
        return "iwdg";
    case ResetCause::kWindowWatchdog:
        return "wwdg";
    case ResetCause::kLowPower:
        return "low-power";
    case ResetCause::kBrownout:
        return "brownout";
    case ResetCause::kUnknown:
        break;
    }
    return "unknown";
}

}  // namespace sn::proto
