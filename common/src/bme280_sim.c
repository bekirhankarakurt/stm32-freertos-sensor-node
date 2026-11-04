/**
 * @file bme280_sim.c
 * @brief BME280 register model and environment generator.
 */
#include "sn/bme280_sim.h"

#include <string.h>

#include "sn/le.h"

/* Calibration from the Bosch datasheet example plus typical humidity terms. */
static const uint8_t k_calib_tp[26] = {
    0x70U, 0x6BU, 0x43U, 0x67U, 0x18U, 0xFCU, /* T1 27504, T2 26435, T3 -1000 */
    0x7DU, 0x8EU, 0x43U, 0xD6U, 0xD0U, 0x0BU, /* P1 36477, P2 -10685, P3 3024 */
    0x27U, 0x0BU, 0x8CU, 0x00U, 0xF9U, 0xFFU, /* P4 2855, P5 140, P6 -7 */
    0x8CU, 0x3CU, 0xF8U, 0xC6U, 0x70U, 0x17U, /* P7 15500, P8 -14600, P9 6000 */
    0x00U, 0x4BU                              /* reserved, H1 75 */
};
static const uint8_t k_calib_h[7] = {0x6AU, 0x01U, 0x00U, 0x13U, 0x29U, 0x03U, 0x1EU};
/* H2 362, H3 0, H4 0x13<<4 | 9 = 313, H5 0x03<<4 | 2 = 50, H6 30 */

typedef int64_t (*eval_fn)(const sn_bme280_calib_t *c, int32_t adc, int32_t t_fine);

static int64_t eval_t(const sn_bme280_calib_t *c, int32_t adc, int32_t t_fine)
{
    int32_t fine = t_fine;
    return (int64_t)sn_bme280_compensate_t(c, adc, &fine);
}

static int64_t eval_p(const sn_bme280_calib_t *c, int32_t adc, int32_t t_fine)
{
    return (int64_t)((sn_bme280_compensate_p(c, adc, t_fine) + 128U) >> 8);
}

static int64_t eval_h(const sn_bme280_calib_t *c, int32_t adc, int32_t t_fine)
{
    return (int64_t)(((uint64_t)sn_bme280_compensate_h(c, adc, t_fine) * 100U + 512U) >> 10);
}

/* Binary search over a monotonic compensation function. */
static int32_t invert(const sn_bme280_calib_t *c, eval_fn f, int32_t t_fine, int64_t target, int32_t lo,
                      int32_t hi)
{
    const bool rising = f(c, hi, t_fine) >= f(c, lo, t_fine);
    int32_t a = lo;
    int32_t b = hi;
    while ((b - a) > 1) {
        const int32_t mid = a + ((b - a) / 2);
        const int64_t v = f(c, mid, t_fine);
        const bool below = rising ? (v < target) : (v > target);
        if (below) {
            a = mid;
        } else {
            b = mid;
        }
    }
    const int64_t ea = f(c, a, t_fine) - target;
    const int64_t eb = f(c, b, t_fine) - target;
    return (((ea < 0) ? -ea : ea) <= ((eb < 0) ? -eb : eb)) ? a : b;
}

void sn_bme280_sim_set_env(sn_bme280_sim_t *sim, const sn_env_t *env)
{
    /* Search windows keep the inversion inside the sensor's operating range. */
    const int32_t adc_t = invert(&sim->calib, eval_t, 0, env->temperature_cdeg, 300000, 700000);
    int32_t t_fine = 0;
    (void)sn_bme280_compensate_t(&sim->calib, adc_t, &t_fine);
    const int32_t adc_p = invert(&sim->calib, eval_p, t_fine, (int64_t)env->pressure_pa, 100000, 700000);
    const int32_t adc_h = invert(&sim->calib, eval_h, t_fine, (int64_t)env->humidity_cpct, 0, 65535);

    const uint32_t up = (uint32_t)adc_p;
    const uint32_t ut = (uint32_t)adc_t;
    sim->regs[0xF7] = (uint8_t)(up >> 12);
    sim->regs[0xF8] = (uint8_t)((up >> 4) & 0xFFU);
    sim->regs[0xF9] = (uint8_t)((up & 0x0FU) << 4);
    sim->regs[0xFA] = (uint8_t)(ut >> 12);
    sim->regs[0xFB] = (uint8_t)((ut >> 4) & 0xFFU);
    sim->regs[0xFC] = (uint8_t)((ut & 0x0FU) << 4);
    sim->regs[0xFD] = (uint8_t)((uint32_t)adc_h >> 8);
    sim->regs[0xFE] = (uint8_t)((uint32_t)adc_h & 0xFFU);
}

static bool sim_read(void *ctx, uint8_t reg, uint8_t *dst, size_t len)
{
    sn_bme280_sim_t *sim = (sn_bme280_sim_t *)ctx;
    if (((size_t)reg + len) > sizeof(sim->regs)) {
        return false;
    }
    if ((reg <= SN_BME280_REG_DATA) && (((size_t)reg + len) > SN_BME280_REG_DATA) && (sim->provider != NULL)) {
        sn_env_t env;
        sim->provider(sim->provider_ctx, &env);
        sn_bme280_sim_set_env(sim, &env);
        ++sim->reads;
    }
    (void)memcpy(dst, &sim->regs[reg], len);
    return true;
}

static bool sim_write(void *ctx, uint8_t reg, uint8_t value)
{
    sn_bme280_sim_t *sim = (sn_bme280_sim_t *)ctx;
    if (reg == SN_BME280_REG_RESET) {
        if (value == SN_BME280_RESET_WORD) {
            sim->regs[SN_BME280_REG_CTRL_HUM] = 0U;
            sim->regs[SN_BME280_REG_CTRL_MEAS] = 0U;
            sim->regs[SN_BME280_REG_CONFIG] = 0U;
        }
        return true;
    }
    if ((reg == SN_BME280_REG_CTRL_HUM) || (reg == SN_BME280_REG_CTRL_MEAS) || (reg == SN_BME280_REG_CONFIG)) {
        sim->regs[reg] = value;
        return true;
    }
    return false;
}

static void sim_delay(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
}

void sn_bme280_sim_init(sn_bme280_sim_t *sim, sn_env_provider_t provider, void *provider_ctx)
{
    (void)memset(sim->regs, 0, sizeof(sim->regs));
    (void)memcpy(&sim->regs[SN_BME280_REG_CALIB0], k_calib_tp, sizeof(k_calib_tp));
    (void)memcpy(&sim->regs[SN_BME280_REG_CALIB26], k_calib_h, sizeof(k_calib_h));
    sim->regs[SN_BME280_REG_ID] = SN_BME280_CHIP_ID;
    sn_bme280_parse_calib(k_calib_tp, k_calib_h, &sim->calib);
    sim->provider = provider;
    sim->provider_ctx = provider_ctx;
    sim->reads = 0U;
    const sn_env_t idle = {2500, 101325U, 4000U};
    sn_bme280_sim_set_env(sim, &idle);
}

sn_bme280_bus_t sn_bme280_sim_bus(sn_bme280_sim_t *sim)
{
    const sn_bme280_bus_t bus = {sim, sim_read, sim_write, sim_delay};
    return bus;
}

static uint32_t lcg_next(sn_env_model_t *m)
{
    m->rng = (m->rng * 1664525U) + 1013904223U;
    return m->rng >> 16;
}

/* Symmetric triangle wave in [-amplitude, +amplitude]. */
static int32_t triangle(uint32_t t_ms, uint32_t period_ms, int32_t amplitude)
{
    const uint32_t phase = t_ms % period_ms;
    const uint32_t half = period_ms / 2U;
    const int64_t a = (int64_t)amplitude;
    int64_t v;
    if (phase < half) {
        v = -a + ((2 * a * (int64_t)phase) / (int64_t)half);
    } else {
        v = a - ((2 * a * (int64_t)(phase - half)) / (int64_t)half);
    }
    return (int32_t)v;
}

static int32_t noise(sn_env_model_t *m, int32_t amplitude)
{
    const int32_t span = (2 * amplitude) + 1;
    return (int32_t)(lcg_next(m) % (uint32_t)span) - amplitude;
}

void sn_env_model_init(sn_env_model_t *m, uint32_t seed)
{
    m->rng = seed;
    m->calls = 0U;
}

void sn_env_model_sample(sn_env_model_t *m, uint32_t now_ms, sn_env_t *env)
{
    /* Phase offsets start every waveform at its mean value. */
    const uint32_t t = now_ms;
    int32_t temp = 2350 + triangle(t + 90000U, 120000U, 150) + noise(m, 4);
    /* An isolated one-sample glitch every 47 samples exercises the median filter. */
    ++m->calls;
    if ((m->calls % 47U) == 23U) {
        temp += 600;
    }
    env->temperature_cdeg = temp;
    env->pressure_pa = (uint32_t)(101325 + triangle(t + 225000U, 300000U, 120) + noise(m, 3));
    env->humidity_cpct = (uint32_t)(4500 + triangle(t + 150000U, 200000U, 400) + noise(m, 6));
}
