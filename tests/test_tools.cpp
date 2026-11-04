/**
 * @file test_tools.cpp
 * @brief Host tool helpers: message formatting, command parsing, image inspection.
 */
#include <string>

#include "format.hpp"
#include "image_fixture.hpp"
#include "sn/protocol.hpp"
#include "sn_test.hpp"
#include "uploader.hpp"

using sn::tools::format_csv;
using sn::tools::format_text;
using sn::tools::parse_command;

namespace {
bool contains(const std::string& s, const char* needle)
{
    return s.find(needle) != std::string::npos;
}
}  // namespace

TEST(format_telemetry_text_and_csv)
{
    std::vector<std::uint8_t> p(sn::proto::kTelemetrySize);
    const sn::proto::Telemetry t{1500U, -105, 101325U, 4512U, -200, 300, 7U, 100U,
                                 sn::proto::kStatusTrialBoot | sn::proto::kStatusSensorFault, 15U};
    REQUIRE(sn::proto::encode(t, 1U, p.data(), p.size()) == p.size());
    const std::string line = format_text(p);
    CHECK(contains(line, "T= -1.05 C"));
    CHECK(contains(line, "P=  1013.25 hPa"));
    CHECK(contains(line, "RH= 45.12 %"));
    CHECK(contains(line, "SENSOR_FAULT,TRIAL"));
    CHECK(format_csv(p) == "1500,-105,101325,4512,-200,300,7,100,3,15");
    CHECK(contains(sn::tools::csv_header(), "uptime_ms"));
    p.pop_back();
    CHECK(format_text(p).empty());
    CHECK(format_csv(p).empty());
}

TEST(format_status_flags_and_other_messages)
{
    std::vector<std::uint8_t> p(sn::proto::kTelemetrySize);
    const sn::proto::Telemetry t{0U, 0, 0U, 0U, 0, 0, 0U, 10U,
                                 sn::proto::kStatusSlotB | sn::proto::kStatusRolledBack | sn::proto::kStatusTxDrops,
                                 0U};
    (void)sn::proto::encode(t, 0U, p.data(), p.size());
    CHECK(contains(format_text(p), "SLOT_B,ROLLED_BACK,TX_DROPS"));

    std::vector<std::uint8_t> h(sn::proto::kHealthSize);
    sn::proto::Health hv{2000U, 125U, 64U, sn::proto::ResetCause::kSoftware, 0U, 0U, 0U, 0U, 0U, 0U};
    (void)sn::proto::encode(hv, 0U, h.data(), h.size());
    CHECK(contains(format_text(h), "cpu=12.5%"));
    CHECK(contains(format_text(h), "reset=software"));
    hv.cpu_load_permille = sn::proto::kCpuLoadUnavailable;
    (void)sn::proto::encode(hv, 0U, h.data(), h.size());
    CHECK(contains(format_text(h), "cpu=n/a"));

    std::vector<std::uint8_t> b(sn::proto::kBootSize);
    const sn::proto::Boot bv{{1U, 0U, 0U, 1U, 2U, 0xABCDEF01U}, sn::proto::ResetCause::kIndependentWatchdog, 1U};
    (void)sn::proto::encode(bv, 0U, b.data(), b.size());
    CHECK(contains(format_text(b), "v1.0.0 slot=B git=abcdef01 reset=iwdg trial_attempts=2 rolled_back=yes"));

    std::vector<std::uint8_t> v(sn::proto::kVersionSize);
    (void)sn::proto::encode_version(bv.version, 0U, v.data(), v.size());
    CHECK(contains(format_text(v), "VERSION v1.0.0"));

    std::vector<std::uint8_t> a(sn::proto::kAckSize);
    (void)sn::proto::encode_ack(SN_MSG_CMD_SET_PERIOD, SN_STATUS_OUT_OF_RANGE, 0U, a.data(), a.size());
    CHECK(contains(format_text(a), "cmd=0x11 status=out-of-range"));
    a[3] = 99U;
    CHECK(contains(format_text(a), "status=?"));
    a.push_back(0U);
    CHECK(format_text(a).empty());

    CHECK(contains(format_text({0x99U, 0U, 1U}), "type=0x99"));
    CHECK(format_text({0x01U}).empty());
    CHECK(format_text({SN_MSG_HEALTH, 0U}).empty());
    CHECK(format_text({SN_MSG_BOOT, 0U}).empty());
    CHECK(format_text({SN_MSG_VERSION, 0U}).empty());
}

TEST(parse_commands)
{
    std::vector<std::uint8_t> p;
    REQUIRE(parse_command("period=250", 3U, p));
    CHECK((p == std::vector<std::uint8_t>{SN_MSG_CMD_SET_PERIOD, 3U, 250U, 0U}));
    REQUIRE(parse_command("version", 4U, p));
    CHECK_EQ(p[0], SN_MSG_CMD_GET_VERSION);
    REQUIRE(parse_command("reboot", 4U, p));
    CHECK_EQ(p[0], SN_MSG_CMD_REBOOT);
    REQUIRE(parse_command("bootloader", 4U, p));
    CHECK_EQ(p[0], SN_MSG_CMD_BOOTLOADER);
    CHECK(!parse_command("period=", 0U, p));
    CHECK(!parse_command("period=12x", 0U, p));
    CHECK(!parse_command("period=70000", 0U, p));
    CHECK(!parse_command("launch", 0U, p));
}

TEST(inspect_image_checks)
{
    std::string err;
    std::uint32_t addr = 0U;
    auto img = make_image(SN_SLOT_B_ADDR, 512U, 1U, 0U, 0U);
    REQUIRE(sn::tools::inspect_image(img, addr, err));
    CHECK_EQ(addr, SN_SLOT_B_ADDR);
    img.push_back(0U);
    CHECK(!sn::tools::inspect_image(img, addr, err));
    img.pop_back();
    img.back() ^= 1U;
    CHECK(!sn::tools::inspect_image(img, addr, err));
    CHECK(!sn::tools::inspect_image(std::vector<std::uint8_t>(16U), addr, err));
    std::vector<std::uint8_t> blank(1024U, 0xFFU);
    CHECK(!sn::tools::inspect_image(blank, addr, err));
}

SN_TEST_MAIN()
