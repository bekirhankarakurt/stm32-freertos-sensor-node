/**
 * @file bme280.c
 * @brief BME280 driver and compensation (Bosch datasheet BST-BME280-DS002, section 4.2.3).
 *
 * The formulas are the official integer ones. Intermediates are evaluated in
 * 64 bits and left shifts of signed values are written as multiplications so
 * the result is identical for in-range inputs and well defined for any input.
 * Right shifts of negative values rely on arithmetic shifting, exactly like
 * the reference code (GCC and Clang guarantee this behaviour).
 */
#include "sn/bme280.h"

#include "sn/le.h"

#define NVM_COPY_RETRIES 10U

void sn_bme280_parse_calib(const uint8_t tp[26], const uint8_t h[7], sn_bme280_calib_t *calib)
{
    calib->t1 = sn_le_get16(&tp[0]);
    calib->t2 = (int16_t)sn_le_get16(&tp[2]);
    calib->t3 = (int16_t)sn_le_get16(&tp[4]);
    calib->p1 = sn_le_get16(&tp[6]);
    calib->p2 = (int16_t)sn_le_get16(&tp[8]);
    calib->p3 = (int16_t)sn_le_get16(&tp[10]);
    calib->p4 = (int16_t)sn_le_get16(&tp[12]);
    calib->p5 = (int16_t)sn_le_get16(&tp[14]);
    calib->p6 = (int16_t)sn_le_get16(&tp[16]);
    calib->p7 = (int16_t)sn_le_get16(&tp[18]);
    calib->p8 = (int16_t)sn_le_get16(&tp[20]);
    calib->p9 = (int16_t)sn_le_get16(&tp[22]);
    calib->h1 = tp[25];
    calib->h2 = (int16_t)sn_le_get16(&h[0]);
    calib->h3 = h[2];
    calib->h4 = (int16_t)(((int16_t)(int8_t)h[3] * 16) | (int16_t)(h[4] & 0x0FU));
    calib->h5 = (int16_t)(((int16_t)(int8_t)h[5] * 16) | (int16_t)(h[4] >> 4));
    calib->h6 = (int8_t)h[6];
}

int32_t sn_bme280_compensate_t(const sn_bme280_calib_t *c, int32_t adc_t, int32_t *t_fine)
{
    const int64_t t1 = (int64_t)c->t1;
    const int64_t var1 = ((((int64_t)adc_t >> 3) - (t1 * 2)) * (int64_t)c->t2) >> 11;
    const int64_t d = ((int64_t)adc_t >> 4) - t1;
    const int64_t var2 = (((d * d) >> 12) * (int64_t)c->t3) >> 14;
    const int64_t fine = var1 + var2;
    *t_fine = (int32_t)fine;
    return (int32_t)((fine * 5 + 128) >> 8);
}

uint32_t sn_bme280_compensate_p(const sn_bme280_calib_t *c, int32_t adc_p, int32_t t_fine)
{
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->p6;
    var2 = var2 + ((var1 * (int64_t)c->p5) * 131072);           /* << 17 */
    var2 = var2 + ((int64_t)c->p4 * 34359738368LL);              /* << 35 */
    var1 = ((var1 * var1 * (int64_t)c->p3) >> 8) + ((var1 * (int64_t)c->p2) * 4096); /* << 12 */
    var1 = ((140737488355328LL + var1) * (int64_t)c->p1) >> 33;  /* (1 << 47) */
    if (var1 == 0) {
        return 0U;
    }
    int64_t p = 1048576 - (int64_t)adc_p;
    p = (((p * 2147483648LL) - var2) * 3125) / var1;             /* << 31 */
    var1 = ((int64_t)c->p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->p8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->p7 * 16);        /* << 4 */
    if (p < 0) {
        return 0U;
    }
    return (uint32_t)p;
}

uint32_t sn_bme280_compensate_h(const sn_bme280_calib_t *c, int32_t adc_h, int32_t t_fine)
{
    int64_t v = (int64_t)t_fine - 76800;
    const int64_t a = (((int64_t)adc_h * 16384) - ((int64_t)c->h4 * 1048576) - ((int64_t)c->h5 * v) + 16384) >> 15;
    const int64_t b = ((((((v * (int64_t)c->h6) >> 10) * (((v * (int64_t)c->h3) >> 11) + 32768)) >> 10) + 2097152) *
                           (int64_t)c->h2 + 8192) >> 14;
    v = a * b;
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int64_t)c->h1) >> 4);
    if (v < 0) {
        v = 0;
    }
    if (v > 419430400) {
        v = 419430400;
    }
    return (uint32_t)(v >> 12);
}

void sn_bme280_convert(const sn_bme280_calib_t *c, const uint8_t raw[8], sn_bme280_data_t *out)
{
    const int32_t adc_p = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((uint32_t)raw[2] >> 4));
    const int32_t adc_t = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((uint32_t)raw[5] >> 4));
    const int32_t adc_h = (int32_t)(((uint32_t)raw[6] << 8) | (uint32_t)raw[7]);
    int32_t t_fine = 0;
    out->temperature_cdeg = sn_bme280_compensate_t(c, adc_t, &t_fine);
    out->pressure_pa = (sn_bme280_compensate_p(c, adc_p, t_fine) + 128U) >> 8;
    /* Q22.10 %RH -> 0.01 %RH with rounding. */
    out->humidity_cpct = (uint32_t)(((uint64_t)sn_bme280_compensate_h(c, adc_h, t_fine) * 100U + 512U) >> 10);
}

sn_bme280_status_t sn_bme280_init(sn_bme280_t *dev, const sn_bme280_bus_t *bus)
{
    dev->bus = bus;
    uint8_t id = 0U;
    if (!bus->read(bus->ctx, SN_BME280_REG_ID, &id, 1U)) {
        return SN_BME280_ERR_BUS;
    }
    if (id != SN_BME280_CHIP_ID) {
        return SN_BME280_ERR_CHIP_ID;
    }
    if (!bus->write(bus->ctx, SN_BME280_REG_RESET, SN_BME280_RESET_WORD)) {
        return SN_BME280_ERR_BUS;
    }
    bus->delay_ms(bus->ctx, 3U);

    uint8_t status = 0x01U;
    uint32_t tries = 0U;
    while ((status & 0x01U) != 0U) {
        if (tries >= NVM_COPY_RETRIES) {
            return SN_BME280_ERR_TIMEOUT;
        }
        if (!bus->read(bus->ctx, SN_BME280_REG_STATUS, &status, 1U)) {
            return SN_BME280_ERR_BUS;
        }
        if ((status & 0x01U) != 0U) {
            bus->delay_ms(bus->ctx, 1U);
        }
        ++tries;
    }

    uint8_t tp[26];
    uint8_t h[7];
    if (!bus->read(bus->ctx, SN_BME280_REG_CALIB0, tp, sizeof(tp)) ||
        !bus->read(bus->ctx, SN_BME280_REG_CALIB26, h, sizeof(h))) {
        return SN_BME280_ERR_BUS;
    }
    sn_bme280_parse_calib(tp, h, &dev->calib);

    /* ctrl_hum must be written before ctrl_meas to take effect. */
    const bool ok = bus->write(bus->ctx, SN_BME280_REG_CTRL_HUM, 0x01U) &&   /* osrs_h x1 */
                    bus->write(bus->ctx, SN_BME280_REG_CONFIG, 0x00U) &&     /* t_sb 0.5 ms, IIR off */
                    bus->write(bus->ctx, SN_BME280_REG_CTRL_MEAS, 0x4FU);    /* osrs_t x2, osrs_p x4, normal */
    return ok ? SN_BME280_OK : SN_BME280_ERR_BUS;
}

sn_bme280_status_t sn_bme280_read(sn_bme280_t *dev, sn_bme280_data_t *out)
{
    uint8_t raw[8];
    if (!dev->bus->read(dev->bus->ctx, SN_BME280_REG_DATA, raw, sizeof(raw))) {
        return SN_BME280_ERR_BUS;
    }
    sn_bme280_convert(&dev->calib, raw, out);
    return SN_BME280_OK;
}
