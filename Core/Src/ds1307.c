#include "ds1307.h"

static I2C_HandleTypeDef *ds_hi2c;

/* DS1307 7-bit I2C address is 0x68.
 * STM32 HAL I2C APIs expect the 7-bit address shifted left by 1.
 * HAL sets the R/W bit internally depending on read/write operation.
 * https://sourcevu.sysprogs.com/stm32/HAL/symbols/HAL_I2C_Mem_Read
 */
#define DS1307_ADDRESS (0x68 << 1)

// BCD to Decimal conversion
static uint8_t bcd2dec(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

// Decimal to BCD conversion
static uint8_t dec2bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

uint8_t DS1307_Init(I2C_HandleTypeDef *hi2c)
{
    ds_hi2c = hi2c;
    uint8_t status = 0;

    // Read the seconds register to check the CH (Clock Halt) bit
    if (HAL_I2C_Mem_Read(ds_hi2c, DS1307_ADDRESS, 0x00, 1, &status, 1, 100) != HAL_OK)
    {
        return 0; // Device not found or error
    }

    // If CH bit is set (bit 7), clock is halted. Clear it to start the clock.
    if (status & 0x80)
    {
        status &= 0x7F; // Clear CH bit
        HAL_I2C_Mem_Write(ds_hi2c, DS1307_ADDRESS, 0x00, 1, &status, 1, 100);
    }

    return 1;
}

uint8_t DS1307_SetTime(DS1307_Time_t *time)
{
    uint8_t buf[7];
    buf[0] = dec2bcd(time->seconds);
    buf[1] = dec2bcd(time->minutes);
    buf[2] = dec2bcd(time->hour); // 24-hour mode
    buf[3] = dec2bcd(time->dayofweek);
    buf[4] = dec2bcd(time->dayofmonth);
    buf[5] = dec2bcd(time->month);
    buf[6] = dec2bcd(time->year);

    if (HAL_I2C_Mem_Write(ds_hi2c, DS1307_ADDRESS, 0x00, 1, buf, 7, 100) != HAL_OK)
    {
        return 0;
    }
    return 1;
}

uint8_t DS1307_GetTime(DS1307_Time_t *time)
{
    uint8_t buf[7];
    if (HAL_I2C_Mem_Read(ds_hi2c, DS1307_ADDRESS, 0x00, 1, buf, 7, 100) != HAL_OK)
    {
        return 0;
    }

    time->seconds = bcd2dec(buf[0] & 0x7F);
    time->minutes = bcd2dec(buf[1]);
    time->hour = bcd2dec(buf[2] & 0x3F); // 24-hour mode
    time->dayofweek = bcd2dec(buf[3]);
    time->dayofmonth = bcd2dec(buf[4]);
    time->month = bcd2dec(buf[5]);
    time->year = bcd2dec(buf[6]);

    return 1;
}