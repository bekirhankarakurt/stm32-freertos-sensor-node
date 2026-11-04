/**
 * @file test_app.cpp
 * @brief Filters, statistics, processing pipeline, protocol codecs, commands and health supervision.
 */
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "sn/command.hpp"
#include "sn/filter.hpp"
#include "sn/health.hpp"
#include "sn/processing.hpp"
#include "sn/protocol.hpp"
#include "sn_test.hpp"

using namespace sn;

TEST(isqrt_exact_and_floor)
{
    CHECK_EQ(isqrt64(0U), 0U);
    CHECK_EQ(isqrt64(1U), 1U);
    CHECK_EQ(isqrt64(15U), 3U);
    CHECK_EQ(isqrt64(16U), 4U);
    CHECK_EQ(isqrt64(0xFFFFFFFFFFFFFFFFULL), 0xFFFFFFFFU);
    for (std::uint64_t v = 1U; v < 2000000U; v = v * 3U + 7U) {
        const std::uint64_t r = isqrt64(v);
        CHECK(r * r <= v);
        CHECK((r + 1U) * (r + 1U) > v);
    }
}

TEST(median3_rejects_single_spikes)
{
    Median3 m;
    CHECK_EQ(m.push(10), 10);
    CHECK_EQ(m.push(11), 11);
    CHECK_EQ(m.push(12), 11);
    CHECK_EQ(m.push(500), 12);
    CHECK_EQ(m.push(13), 13);
    CHECK_EQ(m.push(-400), 13);
    CHECK_EQ(m.push(14), 13);
    CHECK_EQ(m.push(14), 14);
}

TEST(ema_converges_and_rounds)
{
    Ema e(2U);
    CHECK_EQ(e.push(1000), 1000);
    int32_t v = 0;
    for (int i = 0; i < 60; ++i) {
        v = e.push(2000);
    }
    CHECK(v >= 1999 && v <= 2000);
    Ema neg(3U);
    CHECK_EQ(neg.push(-500), -500);
    CHECK(neg.push(-1000) < -500);
    Ema clamp_lo(0U);
    Ema clamp_hi(40U);
    CHECK_EQ(clamp_lo.push(7), 7);
    CHECK_EQ(clamp_hi.push(7), 7);
}

TEST(window_stats_match_brute_force)
{
    WindowStats<8> w;
    CHECK_EQ(w.min(), 0);
    CHECK_EQ(w.max(), 0);
    CHECK_EQ(w.mean(), 0);
    CHECK_EQ(w.stddev(), 0U);
    std::vector<int32_t> hist;
    std::uint32_t seed = 3U;
    for (int i = 0; i < 100; ++i) {
        seed = seed * 1103515245U + 12345U;
        const auto v = static_cast<int32_t>((seed >> 16) % 2001U) - 1000;
        w.push(v);
        hist.push_back(v);
        const std::size_t n = std::min<std::size_t>(hist.size(), 8U);
        double sum = 0.0;
        int32_t lo = hist.back();
        int32_t hi = hist.back();
        for (std::size_t k = hist.size() - n; k < hist.size(); ++k) {
            sum += hist[k];
            lo = std::min(lo, hist[k]);
            hi = std::max(hi, hist[k]);
        }
        const double mean = sum / static_cast<double>(n);
        double var = 0.0;
        for (std::size_t k = hist.size() - n; k < hist.size(); ++k) {
            var += (hist[k] - mean) * (hist[k] - mean);
        }
        const double sd = std::sqrt(var / static_cast<double>(n));
        CHECK_EQ(w.count(), n);
        CHECK_EQ(w.min(), lo);
        CHECK_EQ(w.max(), hi);
        CHECK(std::fabs(w.mean() - mean) <= 0.5);
        CHECK(std::fabs(static_cast<double>(w.stddev()) - sd) <= 1.0);
    }
    WindowStats<4> flat;
    flat.push(5);
    flat.push(5);
    CHECK_EQ(flat.stddev(), 0U);
}

TEST(processor_filters_spikes_and_reports_stats)
{
    SampleProcessor p;
    proto::Telemetry t{};
    RawSample bad{0U, 0, 0U, 0U, false};
    CHECK(!p.process(bad, 100U, 0U, t));
    for (std::uint32_t i = 0U; i < 40U; ++i) {
        const int32_t spike = (i == 20U) ? 3000 : 0;
        const RawSample s{i * 100U, 2300 + spike, 101325U, 4500U, true};
        REQUIRE(p.process(s, 100U, proto::kStatusSlotB, t));
        CHECK(t.temp_cdeg < 2400);
    }
    CHECK_EQ(t.temp_cdeg, 2300);
    CHECK_EQ(t.pressure_pa, 101325U);
    CHECK_EQ(t.humidity_cpct, 4500U);
    CHECK_EQ(t.temp_std_cdeg, 0U);
    CHECK_EQ(t.sample_count, 40U);
    CHECK_EQ(t.status, proto::kStatusSlotB);
    CHECK_EQ(t.uptime_ms, 3900U);
}

TEST(processor_clamps_extreme_values)
{
    SampleProcessor p;
    proto::Telemetry t{};
    const RawSample s{0U, 4000000, 0xFFFFFFFFU, 200000U, true};
    REQUIRE(p.process(s, 10U, 0U, t));
    CHECK_EQ(t.temp_cdeg, 32767);
    CHECK_EQ(t.humidity_cpct, 65535U);
    CHECK_EQ(t.pressure_pa, 0x7FFFFFFFU);
    SampleProcessor q;
    const RawSample cold{0U, -4000000, 0U, 0U, true};
    REQUIRE(q.process(cold, 10U, 0U, t));
    CHECK_EQ(t.temp_cdeg, -32768);
}

TEST(protocol_round_trips)
{
    std::uint8_t buf[64];
    const proto::Telemetry t{123456U, -1234, 101325U, 4567U, -2000, 3000, 42U, 100U, 0x13U, 99U};
    REQUIRE(proto::encode(t, 7U, buf, sizeof(buf)) == proto::kTelemetrySize);
    CHECK_EQ(buf[1], 7);
    proto::Telemetry t2{};
    REQUIRE(proto::decode(buf, proto::kTelemetrySize, t2));
    CHECK_EQ(t2.temp_cdeg, -1234);
    CHECK_EQ(t2.temp_min_cdeg, -2000);
    CHECK_EQ(t2.sample_count, 99U);
    CHECK(!proto::decode(buf, proto::kTelemetrySize - 1U, t2));

    const proto::Health h{5000U, 123U, 77U, proto::ResetCause::kIndependentWatchdog, 1U, 2U, 3U, 4U, 5U, 6U};
    REQUIRE(proto::encode(h, 1U, buf, sizeof(buf)) == proto::kHealthSize);
    proto::Health h2{};
    REQUIRE(proto::decode(buf, proto::kHealthSize, h2));
    CHECK(h2.reset_cause == proto::ResetCause::kIndependentWatchdog);
    CHECK_EQ(h2.wdt_late, 6U);
    buf[10] = 200U;
    REQUIRE(proto::decode(buf, proto::kHealthSize, h2));
    CHECK(h2.reset_cause == proto::ResetCause::kUnknown);

    const proto::Boot b{{1U, 2U, 300U, 1U, 2U, 0xDEADBEEFU}, proto::ResetCause::kPin, 1U};
    REQUIRE(proto::encode(b, 2U, buf, sizeof(buf)) == proto::kBootSize);
    proto::Boot b2{};
    REQUIRE(proto::decode(buf, proto::kBootSize, b2));
    CHECK_EQ(b2.version.patch, 300U);
    CHECK_EQ(b2.version.git_hash, 0xDEADBEEFU);
    CHECK(b2.reset_cause == proto::ResetCause::kPin);

    REQUIRE(proto::encode_version(b.version, 3U, buf, sizeof(buf)) == proto::kVersionSize);
    proto::Version v{};
    REQUIRE(proto::decode_version(buf, proto::kVersionSize, v));
    CHECK_EQ(v.slot, 1U);
    CHECK(!proto::decode(buf, proto::kVersionSize, b2));

    CHECK_EQ(proto::encode(t, 0U, buf, 4U), 0U);
    CHECK_EQ(proto::encode(h, 0U, buf, 4U), 0U);
    CHECK_EQ(proto::encode(b, 0U, buf, 4U), 0U);
    CHECK_EQ(proto::encode_version(v, 0U, buf, 4U), 0U);
    CHECK_EQ(proto::encode_ack(1U, 0U, 0U, buf, 3U), 0U);
}

TEST(reset_cause_names)
{
    CHECK(std::string(proto::to_string(proto::ResetCause::kPowerOn)) == "power-on");
    CHECK(std::string(proto::to_string(proto::ResetCause::kSoftware)) == "software");
    CHECK(std::string(proto::to_string(proto::ResetCause::kWindowWatchdog)) == "wwdg");
    CHECK(std::string(proto::to_string(proto::ResetCause::kLowPower)) == "low-power");
    CHECK(std::string(proto::to_string(proto::ResetCause::kBrownout)) == "brownout");
    CHECK(std::string(proto::to_string(proto::ResetCause::kUnknown)) == "unknown");
}

namespace {
class FakeNode final : public NodeControl {
public:
    void set_period_ms(std::uint16_t p) override { period = p; }
    void request_reboot() override { reboot = true; }
    void request_bootloader() override { bootloader = true; }
    proto::Version version() const override { return proto::Version{1U, 0U, 0U, 0U, 0U, 0x1234U}; }
    std::uint16_t period{0U};
    bool reboot{false};
    bool bootloader{false};
};
}  // namespace

TEST(commands_dispatch_and_validate)
{
    FakeNode node;
    std::uint8_t resp[32];
    const std::uint8_t set[] = {SN_MSG_CMD_SET_PERIOD, 9U, 50U, 0U};
    REQUIRE(handle_command(set, sizeof(set), node, resp, sizeof(resp)) == proto::kAckSize);
    CHECK_EQ(resp[0], SN_MSG_ACK);
    CHECK_EQ(resp[1], 9);
    CHECK_EQ(resp[3], SN_STATUS_OK);
    CHECK_EQ(node.period, 50U);

    const std::uint8_t too_fast[] = {SN_MSG_CMD_SET_PERIOD, 1U, 5U, 0U};
    (void)handle_command(too_fast, sizeof(too_fast), node, resp, sizeof(resp));
    CHECK_EQ(resp[3], SN_STATUS_OUT_OF_RANGE);
    const std::uint8_t too_slow[] = {SN_MSG_CMD_SET_PERIOD, 1U, 0xE9U, 0x03U};
    (void)handle_command(too_slow, sizeof(too_slow), node, resp, sizeof(resp));
    CHECK_EQ(resp[3], SN_STATUS_OUT_OF_RANGE);
    CHECK_EQ(node.period, 50U);

    const std::uint8_t ver[] = {SN_MSG_CMD_GET_VERSION, 4U};
    REQUIRE(handle_command(ver, sizeof(ver), node, resp, sizeof(resp)) == proto::kVersionSize);
    CHECK_EQ(resp[0], SN_MSG_VERSION);

    const std::uint8_t reboot[] = {SN_MSG_CMD_REBOOT, 5U};
    (void)handle_command(reboot, sizeof(reboot), node, resp, sizeof(resp));
    CHECK(node.reboot);
    const std::uint8_t bl[] = {SN_MSG_CMD_BOOTLOADER, 6U};
    (void)handle_command(bl, sizeof(bl), node, resp, sizeof(resp));
    CHECK(node.bootloader);

    const std::uint8_t unknown[] = {0x77U, 1U};
    (void)handle_command(unknown, sizeof(unknown), node, resp, sizeof(resp));
    CHECK_EQ(resp[3], SN_STATUS_UNKNOWN);
    CHECK_EQ(handle_command(unknown, 1U, node, resp, sizeof(resp)), 0U);

    const std::uint8_t long_cmds[][5] = {{SN_MSG_CMD_GET_VERSION, 1U, 0U, 0U, 0U},
                                         {SN_MSG_CMD_SET_PERIOD, 1U, 0U, 0U, 0U},
                                         {SN_MSG_CMD_REBOOT, 1U, 0U, 0U, 0U},
                                         {SN_MSG_CMD_BOOTLOADER, 1U, 0U, 0U, 0U}};
    for (const auto& c : long_cmds) {
        (void)handle_command(c, sizeof(c), node, resp, sizeof(resp));
        CHECK_EQ(resp[3], SN_STATUS_BAD_LENGTH);
    }
}

TEST(health_feeds_only_when_all_tasks_alive)
{
    HealthMonitor h(1500U, 10000U);
    for (std::uint8_t i = 0U; i < static_cast<std::uint8_t>(TaskId::kCount); ++i) {
        h.checkin(static_cast<TaskId>(i), 100U);
    }
    CHECK(h.evaluate(1000U).feed);
    CHECK(!h.evaluate(1601U).feed);
    CHECK_EQ(h.late_cycles(), 1U);
    h.checkin(TaskId::kCount, 1601U);
    for (std::uint8_t i = 0U; i < static_cast<std::uint8_t>(TaskId::kCount); ++i) {
        h.checkin(static_cast<TaskId>(i), 1700U);
    }
    CHECK(h.all_alive(1800U));
    h.checkin(TaskId::kSampler, 0xFFFFFF00U);
    CHECK(!h.all_alive(0x00000010U));
}

TEST(health_confirms_once_after_continuous_health)
{
    HealthMonitor h(1500U, 10000U);
    std::uint32_t confirms = 0U;
    for (std::uint32_t now = 0U; now <= 30000U; now += 250U) {
        if ((now < 4000U) || (now > 6000U)) {
            for (std::uint8_t i = 0U; i < static_cast<std::uint8_t>(TaskId::kCount); ++i) {
                h.checkin(static_cast<TaskId>(i), now);
            }
        }
        const HealthVerdict v = h.evaluate(now);
        if (v.confirm) {
            ++confirms;
            /* The stall between 4.0 s and 6.0 s restarted the 10 s window. */
            CHECK(now >= 15000U);
        }
    }
    CHECK_EQ(confirms, 1U);
}

SN_TEST_MAIN()
