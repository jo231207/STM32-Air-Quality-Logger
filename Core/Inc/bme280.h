#ifndef INC_BME280_H_
#define INC_BME280_H_

#include "main.h"

typedef struct
{
    float temperature;
    float humidity;
    float pressure;
} BME280_Data_t;

uint8_t BME280_Init(I2C_HandleTypeDef *hi2c);
uint8_t BME280_ReadData(BME280_Data_t *data);

#endif /* INC_BME280_H_ */
