/**
 * @file protocol.hpp
 * @brief Application message layouts shared by the firmware and host tools.
 *
 * Every message is `type (u8) | seq (u8) | body`, little-endian, carried by
 * the CRC-16 + COBS framing in sn/frame.h.
 */
#ifndef SN_PROTOCOL_HPP
#define SN_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>

#include "sn/proto_ids.h"

namespace sn::proto {

/** Sampling period limits accepted by ::SN_MSG_CMD_SET_PERIOD. */
inline constexpr std::uint16_t kMinPeriodMs = 10U;
/** Upper sampling period limit. */
inline constexpr std::uint16_t kMaxPeriodMs = 1000U;
/** Default sampling period (10 Hz). */
inline constexpr std::uint16_t kDefaultPeriodMs = 100U;

/** Telemetry status bits. */
enum StatusBits : std::uint16_t {
    kStatusSensorFault = 1U << 0, /**< Last sensor read failed. */
    kStatusTrialBoot = 1U << 1,   /**< Running an unconfirmed image. */
    kStatusSlotB = 1U << 2,       /**< Running from slot B. */
    kStatusRolledBack = 1U << 3,  /**< Bootloader rolled back an update. */
    kStatusTxDrops = 1U << 4      /**< Frames were dropped since last report. */
};

/** Reset causes reported in BOOT and HEALTH. */
enum class ResetCause : std::uint8_t {
    kUnknown = 0, kPowerOn, kPin, kSoftware, kIndependentWatchdog, kWindowWatchdog, kLowPower, kBrownout
};

/** Filtered sample and window statistics. */
struct Telemetry {
    std::uint32_t uptime_ms;      /**< Milliseconds since start. */
    std::int16_t temp_cdeg;       /**< Filtered temperature, 0.01 degC. */
    std::uint32_t pressure_pa;    /**< Filtered pressure, Pa. */
    std::uint16_t humidity_cpct;  /**< Filtered humidity, 0.01 %RH. */
    std::int16_t temp_min_cdeg;   /**< Window minimum. */
    std::int16_t temp_max_cdeg;   /**< Window maximum. */
    std::uint16_t temp_std_cdeg;  /**< Window standard deviation. */
    std::uint16_t period_ms;      /**< Active sampling period. */
    std::uint16_t status;         /**< StatusBits. */
    std::uint32_t sample_count;   /**< Samples processed since start. */
};
/** Encoded telemetry size. */
inline constexpr std::size_t kTelemetrySize = 28U;

/** Periodic health report. */
struct Health {
    std::uint32_t uptime_ms;        /**< Milliseconds since start. */
    std::uint16_t cpu_load_permille; /**< CPU load over the last report, 0.1 %. */
    std::uint16_t min_stack_free;   /**< Smallest stack headroom of any task, words. */
    ResetCause reset_cause;         /**< Cause of the last reset. */
    std::uint8_t boot_flags;        /**< Boot journal flags. */
    std::uint16_t rx_errors;        /**< UART framing/overrun errors. */
    std::uint16_t frame_errors;     /**< Rejected inbound frames. */
    std::uint16_t tx_drops;         /**< Outbound frames dropped (TX full). */
    std::uint16_t sensor_errors;    /**< Failed sensor transactions. */
    std::uint16_t wdt_late;         /**< Health periods with a stalled task. */
};
/** cpu_load_permille value meaning "not measured on this platform". */
inline constexpr std::uint16_t kCpuLoadUnavailable = 0xFFFFU;
/** Encoded health size. */
inline constexpr std::size_t kHealthSize = 22U;

/** Firmware identity (BOOT and VERSION). */
struct Version {
    std::uint8_t major;     /**< Major. */
    std::uint8_t minor;     /**< Minor. */
    std::uint16_t patch;    /**< Patch. */
    std::uint8_t slot;      /**< Running slot (0 = A, 1 = B). */
    std::uint8_t attempts;  /**< Trial boots used (0 when confirmed). */
    std::uint32_t git_hash; /**< Source revision. */
};
/** Encoded version size. */
inline constexpr std::size_t kVersionSize = 12U;

/** Boot announcement. */
struct Boot {
    Version version;         /**< Firmware identity. */
    ResetCause reset_cause;  /**< Cause of this start. */
    std::uint8_t boot_flags; /**< Boot journal flags. */
};
/** Encoded boot size. */
inline constexpr std::size_t kBootSize = 14U;

/** Encoded ACK size. */
inline constexpr std::size_t kAckSize = 4U;

/** Encoders return the payload length, or 0 when @p cap is too small. */
std::size_t encode(const Telemetry& t, std::uint8_t seq, std::uint8_t* out, std::size_t cap);
/** Encode a message; returns the payload length or 0 when @p cap is too small. */
std::size_t encode(const Health& h, std::uint8_t seq, std::uint8_t* out, std::size_t cap);
/** Encode a message; returns the payload length or 0 when @p cap is too small. */
std::size_t encode(const Boot& b, std::uint8_t seq, std::uint8_t* out, std::size_t cap);
/** Encode a VERSION reply. */
std::size_t encode_version(const Version& v, std::uint8_t seq, std::uint8_t* out, std::size_t cap);
/** Encode an ACK for command @p cmd. */
std::size_t encode_ack(std::uint8_t cmd, std::uint8_t status, std::uint8_t seq, std::uint8_t* out,
                       std::size_t cap);

/** Decoders return false on type or length mismatch. */
bool decode(const std::uint8_t* in, std::size_t len, Telemetry& t);
/** Decode a message; false on type or length mismatch. */
bool decode(const std::uint8_t* in, std::size_t len, Health& h);
/** Decode a message; false on type or length mismatch. */
bool decode(const std::uint8_t* in, std::size_t len, Boot& b);
/** Decode a VERSION reply. */
bool decode_version(const std::uint8_t* in, std::size_t len, Version& v);

/** Human-readable reset cause. */
const char* to_string(ResetCause cause);

}  // namespace sn::proto

#endif  // SN_PROTOCOL_HPP
