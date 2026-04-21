/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "hdc1080.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_READING,
    STATE_ALERTA,
    STATE_ERROR
} SystemState_t;

SystemState_t state = STATE_INIT;
HDC1080_Data_t sensorData;
uint16_t ldrValue = 0;
char msg[80];
volatile uint8_t timerFlag = 0;
uint8_t errorCount = 0;        // contador de erros consecutivos
uint32_t lastBlink = 0;        // controla pisca sem HAL_Delay

#define TEMP_MAX     30.0f
#define TEMP_MIN     15.0f
#define MAX_ERRORS   3         // erros consecutivos antes de STATE_ERROR
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Interrupção do TIM6 - dispara a cada 1 segundo
// Prescaler=7199, Period=9999, Clock=72MHz - RM0364 Seção 22
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM6) {
        timerFlag = 1;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
 HAL_Delay(100);
HAL_TIM_Base_Start_IT(&htim6);
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

// Scanner I2C — detecta se o sensor responde
char scan_msg[40];
HAL_UART_Transmit(&huart2, (uint8_t*)"Scan I2C:\r\n", 11, HAL_MAX_DELAY);
uint8_t found = 0;
for (uint8_t addr = 1; addr < 128; addr++) {
    if (HAL_I2C_IsDeviceReady(&hi2c1, addr << 1, 3, 20) == HAL_OK) {
        snprintf(scan_msg, sizeof(scan_msg), "  -> 0x%02X OK\r\n", addr);
        HAL_UART_Transmit(&huart2, (uint8_t*)scan_msg,
                          strlen(scan_msg), HAL_MAX_DELAY);
        found = 1;
    }
}
if (!found) {
    HAL_UART_Transmit(&huart2,
        (uint8_t*)"  Nenhum dispositivo!\r\n", 23, HAL_MAX_DELAY);
}

// Só inicializa o sensor depois do scan
if (HDC1080_Init(&hi2c1) != HAL_OK) {
    state = STATE_ERROR;
    HAL_UART_Transmit(&huart2,
        (uint8_t*)"ERRO: HDC1080!\r\n", 16, HAL_MAX_DELAY);
} else {
    state = STATE_IDLE;
    HAL_UART_Transmit(&huart2,
        (uint8_t*)"Sistema OK!\r\n", 13, HAL_MAX_DELAY);
}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
   switch (state) {

        case STATE_INIT:
            // LED verde ON, vermelho OFF — estado inicial
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
            errorCount = 0;
            state = STATE_IDLE;
            break;

        case STATE_IDLE:
            if (timerFlag) {
                timerFlag = 0;
                state = STATE_READING;
            }
            break;

        case STATE_READING:
            if (HDC1080_ReadData(&hi2c1, &sensorData) != HAL_OK) {
                errorCount++;
                snprintf(msg, sizeof(msg),
                    "Falha na leitura! (%d/%d)\r\n", errorCount, MAX_ERRORS);
                HAL_UART_Transmit(&huart2,
                    (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

                // Só vai para ERROR após várias falhas consecutivas
                if (errorCount >= MAX_ERRORS) {
                    state = STATE_ERROR;
                } else {
                    state = STATE_IDLE; // tenta de novo no próximo ciclo
                }
                break;
            }

            // Leitura OK — zera contador de erros
            errorCount = 0;

            // Leitura do LDR via ADC1 canal IN2 (PA1)
            HAL_ADC_Start(&hadc1);
            HAL_ADC_PollForConversion(&hadc1, 100);
            ldrValue = HAL_ADC_GetValue(&hadc1);
            HAL_ADC_Stop(&hadc1);

            int16_t temp_int = (int16_t)sensorData.temperature;
            uint8_t temp_dec = (uint8_t)((sensorData.temperature - temp_int) * 10);

            int16_t umid_int = (int16_t)sensorData.humidity;
            uint8_t umid_dec = (uint8_t)((sensorData.humidity - umid_int) * 10);

            snprintf(msg, sizeof(msg),
            "Temp: %d.%d C | Umid: %d.%d %% | LDR: %u\r\n",
            temp_int, temp_dec,
            umid_int, umid_dec,
            ldrValue);

            HAL_UART_Transmit(&huart2,
                (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

            if (sensorData.temperature > TEMP_MAX ||
                sensorData.temperature < TEMP_MIN) {
                state = STATE_ALERTA;
            } else {
                // Temperatura OK — verde ON, vermelho OFF
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
                state = STATE_IDLE;
            }
            break;

        case STATE_ALERTA:
            // Temperatura fora do limite — verde OFF, vermelho ON
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
            HAL_UART_Transmit(&huart2,
                (uint8_t*)"ALERTA: Temp fora do limite!\r\n",
                30, HAL_MAX_DELAY);
            state = STATE_IDLE;
            break;

        case STATE_ERROR:
            // Pisca vermelho SEM HAL_Delay — não bloqueia a máquina de estados
            if (HAL_GetTick() - lastBlink >= 300) {
                lastBlink = HAL_GetTick();
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
            }
            // Tenta se recuperar a cada tick do timer
            if (timerFlag) {
                timerFlag = 0;
                if (HDC1080_Init(&hi2c1) == HAL_OK) {
                    HAL_UART_Transmit(&huart2,
                        (uint8_t*)"Sensor recuperado!\r\n", 20, HAL_MAX_DELAY);
                    state = STATE_INIT; // reinicia limpo
                }
            }
            break;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_ADC12;
  PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
