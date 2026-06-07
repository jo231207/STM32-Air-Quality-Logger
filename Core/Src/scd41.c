#include "scd41.h"

static I2C_HandleTypeDef *scd_hi2c;
static const uint16_t SCD41_ADDR = 0x62 << 1;

static uint8_t SCD41_WriteCommand(uint16_t cmd)
{
    uint8_t buf[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return HAL_I2C_Master_Transmit(scd_hi2c, SCD41_ADDR, buf, 2, 100);
}

static uint8_t SCD41_CheckCRC(uint8_t data[2], uint8_t crc)
{
    uint8_t crc_calc = 0xFF;
    for (int i = 0; i < 2; i++)
    {
        crc_calc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc_calc & 0x80)
            {
                crc_calc = (crc_calc << 1) ^ 0x31;
            }
            else
            {
                crc_calc = (crc_calc << 1);
            }
        }
    }
    return crc_calc == crc;
}

uint8_t SCD41_Init(I2C_HandleTypeDef *hi2c)
{
    scd_hi2c = hi2c;
    // Stop periodic measurement just in case it was running
    SCD41_WriteCommand(0x3F86);
    HAL_Delay(500); // It takes up to 500ms to stop periodic measurement

    // Start periodic measurement
    if (SCD41_WriteCommand(0x21B1) != HAL_OK)
    {
        return 0; // Error
    }
    return 1; // Success
}

uint8_t SCD41_ReadData(SCD41_Data_t *data)
{
    // Check Data Ready Status (0xE4B8)
    if (SCD41_WriteCommand(0xE4B8) != HAL_OK)
        return 0;
    HAL_Delay(1);

    uint8_t status_buf[3];
    if (HAL_I2C_Master_Receive(scd_hi2c, SCD41_ADDR, status_buf, 3, 100) != HAL_OK)
        return 0;

    if (!SCD41_CheckCRC(status_buf, status_buf[2]))
        return 0;

    uint16_t status = (status_buf[0] << 8) | status_buf[1];
    if ((status & 0x07FF) == 0)
        return 0; // Data not ready

    // Read Measurement (0xEC05)
    if (SCD41_WriteCommand(0xEC05) != HAL_OK)
        return 0;
    HAL_Delay(1);

    uint8_t read_buf[9];
    if (HAL_I2C_Master_Receive(scd_hi2c, SCD41_ADDR, read_buf, 9, 100) != HAL_OK)
        return 0;

    if (!SCD41_CheckCRC(&read_buf[0], read_buf[2]) || !SCD41_CheckCRC(&read_buf[3], read_buf[5]) ||
        !SCD41_CheckCRC(&read_buf[6], read_buf[8]))
    {
        return 0; // CRC error
    }

    data->co2 = (read_buf[0] << 8) | read_buf[1];

    uint16_t temp_raw = (read_buf[3] << 8) | read_buf[4];
    data->temperature = -45.0f + 175.0f * (float)temp_raw / 65535.0f;

    uint16_t hum_raw = (read_buf[6] << 8) | read_buf[7];
    data->humidity = 100.0f * (float)hum_raw / 65535.0f;

    return 1; // Success
}
