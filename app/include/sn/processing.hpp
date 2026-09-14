/**
 * @file processing.hpp
 * @brief Sample processing pipeline (raw sensor data -> telemetry record).
 */
#ifndef SN_PROCESSING_HPP
#define SN_PROCESSING_HPP

#include <cstdint>

#include "sn/filter.hpp"
#include "sn/protocol.hpp"

namespace sn {

/** One raw sensor reading. */
struct RawSample {
    std::uint32_t timestamp_ms;  /**< Tick at acquisition. */
    std::int32_t temp_cdeg;      /**< 0.01 degC. */
    std::uint32_t pressure_pa;   /**< Pa. */
    std::uint32_t humidity_cpct; /**< 0.01 %RH. */
    bool valid;                  /**< Read succeeded. */
};

/** Filters each channel and maintains temperature window statistics. */
class SampleProcessor {
public:
    /** Window length for the statistics. */
    static constexpr std::size_t kWindow = 32U;

    SampleProcessor();

    /**
     * @brief Process a sample.
     * @param s         Raw sample.
     * @param period_ms Active sampling period (copied into the record).
     * @param status    Status bits to report.
     * @param[out] out  Telemetry record (filled only when @p s is valid).
     * @return true when @p out was updated.
     */
    bool process(const RawSample& s, std::uint16_t period_ms, std::uint16_t status, proto::Telemetry& out);

private:
    ChannelFilter temp_;
    ChannelFilter press_;
    ChannelFilter hum_;
    WindowStats<kWindow> temp_stats_;
    std::uint32_t samples_{0U};
};

}  // namespace sn

#endif  // SN_PROCESSING_HPP
