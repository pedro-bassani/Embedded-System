#ifndef HDC1080_H
#define HDC1080_H

#include "stm32f3xx_hal.h"

// Endereço I2C do HDC1080 - Datasheet TI, Tabela 2
#define HDC1080_ADDR  (0x40 << 1)
#define HDC1080_REG_TEMP    0x00
#define HDC1080_REG_CONFIG  0x02

typedef struct {
    float temperature;  // °C
    float humidity;     // %
} HDC1080_Data_t;

HAL_StatusTypeDef HDC1080_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef HDC1080_ReadData(I2C_HandleTypeDef *hi2c,
                                    HDC1080_Data_t *data);
#endif