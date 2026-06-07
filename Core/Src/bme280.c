#include "bme280.h"

#define BME280_I2C_ADDR_PRIMARY (0x76U << 1)
#define BME280_I2C_ADDR_SECONDARY (0x77U << 1)

#define BME280_CHIP_ID 0x60U

#define BME280_REG_ID 0xD0U
#define BME280_REG_CALIB_TP 0x88U
#define BME280_REG_CALIB_H1 0xA1U
#define BME280_REG_CALIB_H2 0xE1U
#define BME280_REG_CTRL_HUM 0xF2U
#define BME280_REG_CTRL_MEAS 0xF4U
#define BME280_REG_CONFIG 0xF5U
#define BME280_REG_DATA_START 0xF7U

#define BME280_CALIB_TP_LEN 24U
#define BME280_CALIB_H_LEN 7U
#define BME280_DATA_LEN 8U
#define BME280_REG_VALUE_LEN 1U
#define BME280_I2C_TIMEOUT_MS 100U

#define BME280_CTRL_HUM_OSRS_X1 0x01U
#define BME280_CTRL_MEAS_OSRS_X1_NORMAL 0x27U
#define BME280_CONFIG_TSB_1000_FILTER_OFF 0xA0U

#define BME280_SIGN12_MASK 0x0FFFU
#define BME280_SIGN12_BIT 0x0800U
#define BME280_SIGN12_EXT_MASK 0xF000U

static I2C_HandleTypeDef *bme_hi2c;
static uint8_t dev_addr = BME280_I2C_ADDR_PRIMARY;

static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t dig_H1, dig_H3;
static int16_t dig_H2, dig_H4, dig_H5;
static int8_t dig_H6;
static int32_t t_fine;

/**
 * @brief Read an unsigned 16-bit little-endian calibration value.
 */
static uint16_t read_u16_le(const uint8_t *data)
{
    return ((uint16_t)data[1] << 8) | (uint16_t)data[0];
}

/**
 * @brief Read a signed 16-bit little-endian calibration value.
 */
static int16_t read_s16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

/**
 * @brief Sign-extend a 12-bit BME280 humidity calibration value.
 */
static int16_t sign_extend_12bit(uint16_t value)
{
    value &= BME280_SIGN12_MASK;
    if ((value & BME280_SIGN12_BIT) != 0U)
    {
        value |= BME280_SIGN12_EXT_MASK;
    }

    return (int16_t)value;
}

/**
 * @brief Read BME280 register data over I2C.
 */
static HAL_StatusTypeDef read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    if ((bme_hi2c == NULL) || (data == NULL))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(bme_hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len,
                            BME280_I2C_TIMEOUT_MS);
}

/**
 * @brief Write one BME280 register over I2C.
 */
static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t val)
{
    if (bme_hi2c == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Write(bme_hi2c, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, &val,
                             BME280_REG_VALUE_LEN, BME280_I2C_TIMEOUT_MS);
}

uint8_t BME280_Init(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL)
    {
        return 0;
    }

    bme_hi2c = hi2c;
    uint8_t id = 0;

    dev_addr = BME280_I2C_ADDR_PRIMARY;
    if ((read_reg(BME280_REG_ID, &id, BME280_REG_VALUE_LEN) != HAL_OK) || (id != BME280_CHIP_ID))
    {
        dev_addr = BME280_I2C_ADDR_SECONDARY;
        if ((read_reg(BME280_REG_ID, &id, BME280_REG_VALUE_LEN) != HAL_OK) ||
            (id != BME280_CHIP_ID))
        {
            return 0;
        }
    }

    uint8_t calib[BME280_CALIB_TP_LEN];
    if (read_reg(BME280_REG_CALIB_TP, calib, (uint16_t)sizeof(calib)) != HAL_OK)
    {
        return 0;
    }

    if (read_reg(BME280_REG_CALIB_H1, &dig_H1, BME280_REG_VALUE_LEN) != HAL_OK)
    {
        return 0;
    }

    uint8_t calib2[BME280_CALIB_H_LEN];
    if (read_reg(BME280_REG_CALIB_H2, calib2, (uint16_t)sizeof(calib2)) != HAL_OK)
    {
        return 0;
    }

    dig_T1 = read_u16_le(&calib[0]);
    dig_T2 = read_s16_le(&calib[2]);
    dig_T3 = read_s16_le(&calib[4]);

    dig_P1 = read_u16_le(&calib[6]);
    dig_P2 = read_s16_le(&calib[8]);
    dig_P3 = read_s16_le(&calib[10]);
    dig_P4 = read_s16_le(&calib[12]);
    dig_P5 = read_s16_le(&calib[14]);
    dig_P6 = read_s16_le(&calib[16]);
    dig_P7 = read_s16_le(&calib[18]);
    dig_P8 = read_s16_le(&calib[20]);
    dig_P9 = read_s16_le(&calib[22]);

    dig_H2 = read_s16_le(&calib2[0]);
    dig_H3 = calib2[2];
    dig_H4 = sign_extend_12bit(((uint16_t)calib2[3] << 4) | ((uint16_t)calib2[4] & 0x0FU));
    dig_H5 = sign_extend_12bit(((uint16_t)calib2[5] << 4) | ((uint16_t)calib2[4] >> 4));
    dig_H6 = (int8_t)calib2[6];

    /*
     * Datasheet p.30: config writes can be ignored in normal mode, but are
     * accepted in sleep mode. Configure standby/filter before ctrl_meas enters
     * normal mode; ctrl_hum becomes effective after ctrl_meas is written.
     */
    if (write_reg(BME280_REG_CONFIG, BME280_CONFIG_TSB_1000_FILTER_OFF) != HAL_OK)
    {
        return 0;
    }

    if (write_reg(BME280_REG_CTRL_HUM, BME280_CTRL_HUM_OSRS_X1) != HAL_OK)
    {
        return 0;
    }

    if (write_reg(BME280_REG_CTRL_MEAS, BME280_CTRL_MEAS_OSRS_X1_NORMAL) != HAL_OK)
    {
        return 0;
    }

    return 1;
}

uint8_t BME280_ReadData(BME280_Data_t *data)
{
    if ((data == NULL) || (bme_hi2c == NULL))
    {
        return 0;
    }

    uint8_t buffer[BME280_DATA_LEN];
    if (read_reg(BME280_REG_DATA_START, buffer, (uint16_t)sizeof(buffer)) != HAL_OK)
        return 0;

    int32_t adc_p =
        ((int32_t)buffer[0] << 12) | ((int32_t)buffer[1] << 4) | ((int32_t)buffer[2] >> 4);
    int32_t adc_t =
        ((int32_t)buffer[3] << 12) | ((int32_t)buffer[4] << 4) | ((int32_t)buffer[5] >> 4);
    int32_t adc_h = ((int32_t)buffer[6] << 8) | (int32_t)buffer[7];

    // Temperature compensation
    int32_t var1_t = ((((adc_t >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2_t =
        (((((adc_t >> 4) - ((int32_t)dig_T1)) * ((adc_t >> 4) - ((int32_t)dig_T1))) >> 12) *
         ((int32_t)dig_T3)) >>
        14;
    t_fine = var1_t + var2_t;
    data->temperature = (t_fine * 5 + 128) >> 8;
    data->temperature /= 100.0f;

    // Pressure compensation
    int64_t var1_p, var2_p, p;
    var1_p = ((int64_t)t_fine) - 128000;
    var2_p = var1_p * var1_p * (int64_t)dig_P6;
    var2_p = var2_p + ((var1_p * (int64_t)dig_P5) << 17);
    var2_p = var2_p + (((int64_t)dig_P4) << 35);
    var1_p = ((var1_p * var1_p * (int64_t)dig_P3) >> 8) + ((var1_p * (int64_t)dig_P2) << 12);
    var1_p = (((((int64_t)1) << 47) + var1_p)) * ((int64_t)dig_P1) >> 33;
    if (var1_p == 0)
        return 0;
    p = 1048576 - adc_p;
    p = (((p << 31) - var2_p) * 3125) / var1_p;
    var1_p = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2_p = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1_p + var2_p) >> 8) + (((int64_t)dig_P7) << 4);
    data->pressure = (uint32_t)p / 256.0f / 100.0f;

    // Humidity compensation
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_h << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) +
                   ((int32_t)16384)) >>
                  15) *
                 (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >>
                     10) +
                    ((int32_t)2097152)) *
                       ((int32_t)dig_H2) +
                   8192) >>
                  14));
    v_x1_u32r =
        (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    data->humidity = (uint32_t)(v_x1_u32r >> 12) / 1024.0f;

    return 1;
}
