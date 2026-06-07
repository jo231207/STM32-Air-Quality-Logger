#ifndef INC_SCD41_H_
#define INC_SCD41_H_

#include "main.h"

typedef struct
{
    uint16_t co2;
    float temperature;
    float humidity;
} SCD41_Data_t;

uint8_t SCD41_Init(I2C_HandleTypeDef *hi2c);
uint8_t SCD41_ReadData(SCD41_Data_t *data);

#endif /* INC_SCD41_H_ */
