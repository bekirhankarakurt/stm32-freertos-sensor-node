/**
 * @file bme280_sim.h
 * @brief Register-level BME280 model used by the host simulation and the QEMU build.
 *
 * Measurement registers are synthesised from physical target values by
 * numerically inverting the official compensation, so the real driver and
 * its compensation code are exercised end-to-end.
 */
#ifndef SN_BME280_SIM_H
#define SN_BME280_SIM_H

#include <stdbool.h>
#include <stdint.h>

#include "sn/bme280.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Physical environment presented by the model. */
typedef struct {
    int32_t temperature_cdeg; /**< 0.01 degC. */
    uint32_t pressure_pa;     /**< Pa. */
    uint32_t humidity_cpct;   /**< 0.01 %RH. */
} sn_env_t;

/** Callback supplying the current environment when a measurement is read. */
typedef void (*sn_env_provider_t)(void *ctx, sn_env_t *env);

/** Simulated device. */
typedef struct {
    uint8_t regs[256];          /**< Register file. */
    sn_bme280_calib_t calib;    /**< Calibration encoded in @ref regs. */
    sn_env_provider_t provider; /**< Environment source. */
    void *provider_ctx;         /**< Provider context. */
    uint32_t reads;             /**< Burst reads served. */
} sn_bme280_sim_t;

/** @brief Initialise the model with datasheet-typical calibration. */
void sn_bme280_sim_init(sn_bme280_sim_t *sim, sn_env_provider_t provider, void *provider_ctx);

/** @brief Bus interface backed by @p sim (delay is a no-op). */
sn_bme280_bus_t sn_bme280_sim_bus(sn_bme280_sim_t *sim);

/** @brief Encode @p env into the measurement registers. */
void sn_bme280_sim_set_env(sn_bme280_sim_t *sim, const sn_env_t *env);

/** Deterministic environment generator (drift, noise, occasional spikes). */
typedef struct {
    uint32_t rng;   /**< LCG state. */
    uint32_t calls; /**< Samples generated. */
} sn_env_model_t;

/** @brief Seed the generator. */
void sn_env_model_init(sn_env_model_t *m, uint32_t seed);

/** @brief Environment at absolute time @p now_ms. */
void sn_env_model_sample(sn_env_model_t *m, uint32_t now_ms, sn_env_t *env);

#ifdef __cplusplus
}
#endif

#endif /* SN_BME280_SIM_H */
