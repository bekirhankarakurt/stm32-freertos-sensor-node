/**
 * @file processing.cpp
 * @brief Sample processing pipeline.
 */
#include "sn/processing.hpp"

#include <limits>

namespace sn {
namespace {

constexpr std::uint8_t kTempShift = 2U;
constexpr std::uint8_t kPressShift = 3U;
constexpr std::uint8_t kHumShift = 2U;

std::int16_t clamp_i16(std::int32_t v)
{
    constexpr std::int32_t lo = std::numeric_limits<std::int16_t>::min();
    constexpr std::int32_t hi = std::numeric_limits<std::int16_t>::max();
    return static_cast<std::int16_t>((v < lo) ? lo : ((v > hi) ? hi : v));
}

std::uint16_t clamp_u16(std::uint32_t v)
{
    constexpr std::uint32_t hi = std::numeric_limits<std::uint16_t>::max();
    return static_cast<std::uint16_t>((v > hi) ? hi : v);
}

std::int32_t to_i32(std::uint32_t v)
{
    constexpr auto hi = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    return static_cast<std::int32_t>((v > hi) ? hi : v);
}

std::uint32_t to_u32(std::int32_t v)
{
    return (v < 0) ? 0U : static_cast<std::uint32_t>(v);
}

}  // namespace

SampleProcessor::SampleProcessor() : temp_(kTempShift), press_(kPressShift), hum_(kHumShift) {}

bool SampleProcessor::process(const RawSample& s, std::uint16_t period_ms, std::uint16_t status,
                              proto::Telemetry& out)
{
    if (!s.valid) {
        return false;
    }
    const std::int32_t t = temp_.push(s.temp_cdeg);
    const std::int32_t p = press_.push(to_i32(s.pressure_pa));
    const std::int32_t h = hum_.push(to_i32(s.humidity_cpct));
    temp_stats_.push(t);
    ++samples_;

    out.uptime_ms = s.timestamp_ms;
    out.temp_cdeg = clamp_i16(t);
    out.pressure_pa = to_u32(p);
    out.humidity_cpct = clamp_u16(to_u32(h));
    out.temp_min_cdeg = clamp_i16(temp_stats_.min());
    out.temp_max_cdeg = clamp_i16(temp_stats_.max());
    out.temp_std_cdeg = clamp_u16(temp_stats_.stddev());
    out.period_ms = period_ms;
    out.status = status;
    out.sample_count = samples_;
    return true;
}

}  // namespace sn
