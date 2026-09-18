/**
 * @file test_bme280.cpp
 * @brief BME280 compensation against datasheet references, driver init and the register model.
 */
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "sn/bme280.h"
#include "sn/bme280_sim.h"
#include "sn_test.hpp"

namespace {

sn_bme280_calib_t sim_calib()
{
    sn_bme280_sim_t sim;
    sn_bme280_sim_init(&sim, nullptr, nullptr);
    return sim.calib;
}

/* Floating-point reference formulas from the datasheet (section 8.1). */
double ref_temperature(const sn_bme280_calib_t& c, double adc_t, double& t_fine)
{
    const double v1 = (adc_t / 16384.0 - c.t1 / 1024.0) * c.t2;
    const double v2 = (adc_t / 131072.0 - c.t1 / 8192.0) * (adc_t / 131072.0 - c.t1 / 8192.0) * c.t3;
    t_fine = v1 + v2;
    return t_fine / 5120.0;
}

double ref_humidity(const sn_bme280_calib_t& c, double adc_h, double t_fine)
{
    double h = t_fine - 76800.0;
    h = (adc_h - (c.h4 * 64.0 + c.h5 / 16384.0 * h)) *
        (c.h2 / 65536.0 * (1.0 + c.h6 / 67108864.0 * h * (1.0 + c.h3 / 67108864.0 * h)));
    h = h * (1.0 - c.h1 * h / 524288.0);
    return (h > 100.0) ? 100.0 : ((h < 0.0) ? 0.0 : h);
}

struct EnvBox {
    sn_env_t env;
};

void provide(void* ctx, sn_env_t* env)
{
    *env = static_cast<EnvBox*>(ctx)->env;
}

/** Bus wrapper that can fail selected transactions. */
struct FaultyBus {
    sn_bme280_bus_t inner;
    int fail_read_at{-1};
    int fail_write_at{-1};
    int reads{0};
    int writes{0};
    std::uint8_t status_override{0U};
    bool force_busy{false};
};

bool faulty_read(void* ctx, std::uint8_t reg, std::uint8_t* dst, std::size_t len)
{
    auto* f = static_cast<FaultyBus*>(ctx);
    if (f->reads++ == f->fail_read_at) {
        return false;
    }
    const bool ok = f->inner.read(f->inner.ctx, reg, dst, len);
    if (f->force_busy && (reg == SN_BME280_REG_STATUS)) {
        dst[0] = 0x01U;
    }
    return ok;
}

bool faulty_write(void* ctx, std::uint8_t reg, std::uint8_t v)
{
    auto* f = static_cast<FaultyBus*>(ctx);
    if (f->writes++ == f->fail_write_at) {
        return false;
    }
    return f->inner.write(f->inner.ctx, reg, v);
}

void no_delay(void* ctx, std::uint32_t ms)
{
    (void)ctx;
    (void)ms;
}

}  // namespace

TEST(datasheet_temperature_and_pressure_example)
{
    const sn_bme280_calib_t c = sim_calib();
    int32_t t_fine = 0;
    CHECK_EQ(sn_bme280_compensate_t(&c, 519888, &t_fine), 2508);
    CHECK_EQ(t_fine, 128422);
    const std::uint32_t p_q24_8 = sn_bme280_compensate_p(&c, 415148, t_fine);
    CHECK_EQ(p_q24_8 / 256U, 100653U);
}

TEST(temperature_matches_float_reference)
{
    const sn_bme280_calib_t c = sim_calib();
    for (int32_t adc = 380000; adc <= 650000; adc += 1357) {
        int32_t t_fine = 0;
        double ref_fine = 0.0;
        const double ref = ref_temperature(c, adc, ref_fine);
        const double got = sn_bme280_compensate_t(&c, adc, &t_fine) / 100.0;
        CHECK(std::fabs(got - ref) < 0.011);
    }
}

TEST(humidity_matches_float_reference)
{
    const sn_bme280_calib_t c = sim_calib();
    int32_t t_fine = 0;
    (void)sn_bme280_compensate_t(&c, 519888, &t_fine);
    double ref_fine = 0.0;
    (void)ref_temperature(c, 519888, ref_fine);
    for (int32_t adc = 20000; adc <= 45000; adc += 499) {
        const double ref = ref_humidity(c, adc, ref_fine);
        const double got = sn_bme280_compensate_h(&c, adc, t_fine) / 1024.0;
        CHECK(std::fabs(got - ref) < 0.05);
    }
    CHECK_EQ(sn_bme280_compensate_h(&c, 0, t_fine), 0U);
    CHECK_EQ(sn_bme280_compensate_h(&c, 65535, t_fine), 419430400U >> 12);
}

TEST(pressure_degenerate_calibration_returns_zero)
{
    sn_bme280_calib_t c = sim_calib();
    c.p1 = 0U;
    CHECK_EQ(sn_bme280_compensate_p(&c, 415148, 128422), 0U);
}

TEST(calibration_parse_signed_nibbles)
{
    const std::uint8_t tp[26] = {};
    const std::uint8_t h[7] = {0x00, 0x00, 0x00, 0xF0, 0x7A, 0x80, 0xFF};
    sn_bme280_calib_t c{};
    sn_bme280_parse_calib(tp, h, &c);
    CHECK_EQ(c.h4, -256 + 0x0A);
    CHECK_EQ(c.h5, -2048 + 0x07);
    CHECK_EQ(c.h6, -1);
}

TEST(simulator_round_trip_across_operating_range)
{
    EnvBox box{};
    sn_bme280_sim_t sim;
    sn_bme280_sim_init(&sim, provide, &box);
    const sn_bme280_bus_t bus = sn_bme280_sim_bus(&sim);
    sn_bme280_t dev{};
    REQUIRE(sn_bme280_init(&dev, &bus) == SN_BME280_OK);
    for (int32_t t = -2000; t <= 6000; t += 777) {
        for (std::uint32_t p = 80000U; p <= 110000U; p += 7500U) {
            for (std::uint32_t h = 1000U; h <= 9000U; h += 2600U) {
                box.env = sn_env_t{t, p, h};
                sn_bme280_data_t d{};
                REQUIRE(sn_bme280_read(&dev, &d) == SN_BME280_OK);
                CHECK(std::abs(d.temperature_cdeg - t) <= 1);
                CHECK(std::abs(static_cast<int>(d.pressure_pa) - static_cast<int>(p)) <= 2);
                CHECK(std::abs(static_cast<int>(d.humidity_cpct) - static_cast<int>(h)) <= 2);
            }
        }
    }
    CHECK(sim.reads > 0U);
}

TEST(driver_rejects_wrong_chip_and_bus_errors)
{
    sn_bme280_sim_t sim;
    sn_bme280_sim_init(&sim, nullptr, nullptr);
    FaultyBus fb{sn_bme280_sim_bus(&sim)};
    const sn_bme280_bus_t bus{&fb, faulty_read, faulty_write, no_delay};
    sn_bme280_t dev{};

    sim.regs[SN_BME280_REG_ID] = 0x58U; /* BMP280 */
    CHECK_EQ(sn_bme280_init(&dev, &bus), SN_BME280_ERR_CHIP_ID);
    sim.regs[SN_BME280_REG_ID] = SN_BME280_CHIP_ID;

    for (int i = 0; i < 3; ++i) {
        fb.reads = 0;
        fb.fail_read_at = i;
        CHECK_EQ(sn_bme280_init(&dev, &bus), SN_BME280_ERR_BUS);
    }
    fb.fail_read_at = -1;
    for (int i = 0; i < 4; ++i) {
        fb.writes = 0;
        fb.fail_write_at = i;
        CHECK_EQ(sn_bme280_init(&dev, &bus), SN_BME280_ERR_BUS);
    }
    fb.fail_write_at = -1;
    fb.force_busy = true;
    CHECK_EQ(sn_bme280_init(&dev, &bus), SN_BME280_ERR_TIMEOUT);
    fb.force_busy = false;
    REQUIRE(sn_bme280_init(&dev, &bus) == SN_BME280_OK);
    fb.reads = 0;
    fb.fail_read_at = 0;
    sn_bme280_data_t d{};
    CHECK_EQ(sn_bme280_read(&dev, &d), SN_BME280_ERR_BUS);
}

TEST(simulator_register_semantics)
{
    sn_bme280_sim_t sim;
    sn_bme280_sim_init(&sim, nullptr, nullptr);
    sn_bme280_bus_t bus = sn_bme280_sim_bus(&sim);
    std::uint8_t v = 0U;
    CHECK(bus.write(bus.ctx, SN_BME280_REG_CTRL_MEAS, 0x27U));
    CHECK(bus.read(bus.ctx, SN_BME280_REG_CTRL_MEAS, &v, 1U));
    CHECK_EQ(v, 0x27);
    CHECK(bus.write(bus.ctx, SN_BME280_REG_RESET, SN_BME280_RESET_WORD));
    CHECK(bus.read(bus.ctx, SN_BME280_REG_CTRL_MEAS, &v, 1U));
    CHECK_EQ(v, 0x00);
    CHECK(bus.write(bus.ctx, SN_BME280_REG_RESET, 0x12U));
    CHECK(!bus.write(bus.ctx, SN_BME280_REG_ID, 0x00U));
    std::uint8_t big[4];
    CHECK(!bus.read(bus.ctx, 0xFEU, big, sizeof(big)));
    bus.delay_ms(bus.ctx, 1U);
}

TEST(environment_model_is_deterministic_with_isolated_glitches)
{
    sn_env_model_t a;
    sn_env_model_t b;
    sn_env_model_init(&a, 42U);
    sn_env_model_init(&b, 42U);
    int glitches = 0;
    int prev_glitch = -10;
    for (int i = 0; i < 500; ++i) {
        sn_env_t ea{};
        sn_env_t eb{};
        const auto now = static_cast<std::uint32_t>(i * 100);
        sn_env_model_sample(&a, now, &ea);
        sn_env_model_sample(&b, now, &eb);
        CHECK_EQ(ea.temperature_cdeg, eb.temperature_cdeg);
        CHECK(ea.pressure_pa >= 101200U && ea.pressure_pa <= 101450U);
        CHECK(ea.humidity_cpct >= 4090U && ea.humidity_cpct <= 4910U);
        if (ea.temperature_cdeg > 2600) {
            ++glitches;
            CHECK(i - prev_glitch > 1);
            prev_glitch = i;
        }
    }
    CHECK(glitches >= 10);
}

SN_TEST_MAIN()
