#ifndef INC_DS1307_H_
#define INC_DS1307_H_

#include "main.h"

typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hour;
    uint8_t dayofweek;
    uint8_t dayofmonth;
    uint8_t month;
    uint8_t year;
} DS1307_Time_t;

uint8_t DS1307_Init(I2C_HandleTypeDef *hi2c);
uint8_t DS1307_SetTime(DS1307_Time_t *time);
uint8_t DS1307_GetTime(DS1307_Time_t *time);

#endif /* INC_DS1307_H_ */