/*
 * pms7003.h
 *
 *  Created on: Sep 4, 2025
 *      Author: User
 */

#ifndef INC_PMS7003_H_
#define INC_PMS7003_H_

#include "main.h"
#include <stdbool.h>

// PMS7003 active mode frame, 32 bytes total.
// Byte 0     : 0x42 start byte 1
// Byte 1     : 0x4D start byte 2
// Byte 2-3   : frame length, fixed 0x001C
// Byte 4-5   : Data 1, PM1.0 concentration, CF=1 standard particle, ug/m3
// Byte 6-7   : Data 2, PM2.5 concentration, CF=1 standard particle, ug/m3
// Byte 8-9   : Data 3, PM10 concentration, CF=1 standard particle, ug/m3
// Byte 10-11 : Data 4, PM1.0 concentration, atmospheric environment, ug/m3
// Byte 12-13 : Data 5, PM2.5 concentration, atmospheric environment, ug/m3
// Byte 14-15 : Data 6, PM10 concentration, atmospheric environment, ug/m3
// Byte 16-17 : Data 7, particles > 0.3 um in 0.1 L air
// Byte 18-19 : Data 8, particles > 0.5 um in 0.1 L air
// Byte 20-21 : Data 9, particles > 1.0 um in 0.1 L air
// Byte 22-23 : Data 10, particles > 2.5 um in 0.1 L air
// Byte 24-25 : Data 11, particles > 5.0 um in 0.1 L air
// Byte 26-27 : Data 12, particles > 10 um in 0.1 L air
// Byte 28-29 : Data 13, reserved
// Byte 30-31 : checksum, sum of bytes 0 through 29
typedef struct
{
    uint16_t pm1_0_cf1;
    uint16_t pm2_5_cf1;
    uint16_t pm10_0_cf1;
    uint16_t pm1_0_atm;
    uint16_t pm2_5_atm;
    uint16_t pm10_0_atm;
    uint16_t particles_0_3um;
    uint16_t particles_0_5um;
    uint16_t particles_1_0um;
    uint16_t particles_2_5um;
    uint16_t particles_5_0um;
    uint16_t particles_10_0um;
    uint16_t reserved;
} PMS_Data_t;

// 함수 원형
void pms_init(void);
void pms_uart_callback(void); // UART 콜백에서 호출될 함수
bool pms_is_data_ready(void);
PMS_Data_t pms_get_data(void);
bool PMS7003_ReadData(PMS_Data_t *data);
#endif /* INC_PMS7003_H_ */
