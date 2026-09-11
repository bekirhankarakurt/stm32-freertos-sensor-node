/**
 * @file bme280.h
 * @brief Bosch BME280 driver (bus-agnostic) with the official integer compensation.
 *
 * The transport (I2C, SPI or the simulator) is injected through
 * ::sn_bme280_bus_t so the same code runs on target, in QEMU and on the host.
 */
#ifndef SN_BME280_H
#define SN_BME280_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SN_BME280_CHIP_ID     0x60U /**< Expected value of register 0xD0. */
#define SN_BME280_REG_CALIB0  0x88U /**< First T/P calibration register. */
#define SN_BME280_REG_H1      0xA1U /**< dig_H1. */
#define SN_BME280_REG_ID      0xD0U /**< Chip identifier. */
#define SN_BME280_REG_RESET   0xE0U /**< Soft reset. */
#define SN_BME280_REG_CALIB26 0xE1U /**< First humidity calibration register. */
#define SN_BME280_REG_CTRL_HUM 0xF2U /**< Humidity oversampling. */
#define SN_BME280_REG_STATUS  0xF3U /**< measuring / im_update. */
#define SN_BME280_REG_CTRL_MEAS 0xF4U /**< T/P oversampling + mode. */
#define SN_BME280_REG_CONFIG  0xF5U /**< Standby, IIR filter. */
#define SN_BME280_REG_DATA    0xF7U /**< Start of the 8-byte burst. */
#define SN_BME280_RESET_WORD  0xB6U /**< Soft-reset command. */

/** Register-level transport. */
typedef struct {
    void *ctx; /**< Transport context. */
    /** Burst-read @p len bytes starting at @p reg. */
    bool (*read)(void *ctx, uint8_t reg, uint8_t *dst, size_t len);
    /** Write one register. */
    bool (*write)(void *ctx, uint8_t reg, uint8_t value);
    /** Busy/RTOS delay. */
    void (*delay_ms)(void *ctx, uint32_t ms);
} sn_bme280_bus_t;

/** Factory calibration (datasheet section 4.2.2). */
typedef struct {
    uint16_t t1; /**< dig_T1 */
    int16_t t2;  /**< dig_T2 */
    int16_t t3;  /**< dig_T3 */
    uint16_t p1; /**< dig_P1 */
    int16_t p2;  /**< dig_P2 */
    int16_t p3;  /**< dig_P3 */
    int16_t p4;  /**< dig_P4 */
    int16_t p5;  /**< dig_P5 */
    int16_t p6;  /**< dig_P6 */
    int16_t p7;  /**< dig_P7 */
    int16_t p8;  /**< dig_P8 */
    int16_t p9;  /**< dig_P9 */
    uint8_t h1;  /**< dig_H1 */
    int16_t h2;  /**< dig_H2 */
    uint8_t h3;  /**< dig_H3 */
    int16_t h4;  /**< dig_H4 (12-bit signed) */
    int16_t h5;  /**< dig_H5 (12-bit signed) */
    int8_t h6;   /**< dig_H6 */
} sn_bme280_calib_t;

/** Compensated measurement. */
typedef struct {
    int32_t temperature_cdeg; /**< Temperature in 0.01 degC. */
    uint32_t pressure_pa;     /**< Pressure in Pa. */
    uint32_t humidity_cpct;   /**< Relative humidity in 0.01 %RH. */
} sn_bme280_data_t;

/** Driver status. */
typedef enum {
    SN_BME280_OK = 0,      /**< Success. */
    SN_BME280_ERR_BUS,     /**< Transport failure. */
    SN_BME280_ERR_CHIP_ID, /**< Wrong device on the bus. */
    SN_BME280_ERR_TIMEOUT  /**< NVM copy did not finish. */
} sn_bme280_status_t;

/** Driver instance. */
typedef struct {
    const sn_bme280_bus_t *bus; /**< Transport. */
    sn_bme280_calib_t calib;    /**< Calibration read at init. */
} sn_bme280_t;

/** @brief Parse calibration from registers 0x88..0xA1 (26 bytes) and 0xE1..0xE7 (7 bytes). */
void sn_bme280_parse_calib(const uint8_t tp[26], const uint8_t h[7], sn_bme280_calib_t *calib);

/** @brief Temperature compensation; returns 0.01 degC and the shared t_fine. */
int32_t sn_bme280_compensate_t(const sn_bme280_calib_t *c, int32_t adc_t, int32_t *t_fine);

/** @brief Pressure compensation (64-bit variant); returns Pa in Q24.8. */
uint32_t sn_bme280_compensate_p(const sn_bme280_calib_t *c, int32_t adc_p, int32_t t_fine);

/** @brief Humidity compensation; returns %RH in Q22.10. */
uint32_t sn_bme280_compensate_h(const sn_bme280_calib_t *c, int32_t adc_h, int32_t t_fine);

/** @brief Convert one raw 8-byte burst (0xF7..0xFE) into engineering units. */
void sn_bme280_convert(const sn_bme280_calib_t *c, const uint8_t raw[8], sn_bme280_data_t *out);

/**
 * @brief Probe, reset and configure the sensor (normal mode, T x2, P x4, H x1, 0.5 ms standby).
 */
sn_bme280_status_t sn_bme280_init(sn_bme280_t *dev, const sn_bme280_bus_t *bus);

/** @brief Read and compensate the latest measurement. */
sn_bme280_status_t sn_bme280_read(sn_bme280_t *dev, sn_bme280_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SN_BME280_H */
