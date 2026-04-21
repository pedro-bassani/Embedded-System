#include "hdc1080.h"

// Inicializa o sensor - Datasheet TI, Seção 7.5
HAL_StatusTypeDef HDC1080_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t config[3];
    config[0] = HDC1080_REG_CONFIG;
    config[1] = 0x10;
    config[2] = 0x00;

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c, HDC1080_ADDR,
                                   config, 3, HAL_MAX_DELAY);
    HAL_Delay(15); // aguarda sensor estabilizar após configuração
    return ret;
}

// Lê temperatura e umidade - Datasheet TI, Seção 7.4.3
HAL_StatusTypeDef HDC1080_ReadData(I2C_HandleTypeDef *hi2c,
                                    HDC1080_Data_t *data) {
    uint8_t cmd = HDC1080_REG_TEMP;
    uint8_t buf[4];

    // Passo 1: envia comando de leitura
    if (HAL_I2C_Master_Transmit(hi2c, HDC1080_ADDR,
        &cmd, 1, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    // Passo 2: aguarda conversão completa - Datasheet TI, Tabela 2
    HAL_Delay(15);

    // Passo 3: lê os 4 bytes
    if (HAL_I2C_Master_Receive(hi2c, HDC1080_ADDR,
        buf, 4, HAL_MAX_DELAY) != HAL_OK)
        return HAL_ERROR;

    uint16_t raw_temp  = (buf[0] << 8) | buf[1];
    uint16_t raw_humid = (buf[2] << 8) | buf[3];

    // Fórmulas - Datasheet TI, Seção 7.3
    data->temperature = ((float)raw_temp  / 65536.0f) * 165.0f - 40.0f;
    data->humidity    = ((float)raw_humid / 65536.0f) * 100.0f;

    return HAL_OK;
}