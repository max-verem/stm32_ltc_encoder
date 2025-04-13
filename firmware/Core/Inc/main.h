/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define LTC_IN_OK_Pin GPIO_PIN_1
#define LTC_IN_OK_GPIO_Port GPIOB
#define TP1_Pin GPIO_PIN_12
#define TP1_GPIO_Port GPIOB
#define TP2_Pin GPIO_PIN_13
#define TP2_GPIO_Port GPIOB
#define LTC_OUT_Pin GPIO_PIN_15
#define LTC_OUT_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

#define TIMER1_PRESCALER_RAW 48
#define TIMER1_PERIOD_RAW 250
#define TIMER1_PRESCALER (TIMER1_PRESCALER_RAW - 1)
#define TIMER1_PERIOD (TIMER1_PERIOD_RAW - 1)

#define TIMER3_PRESCALER_RAW 48
#define TIMER3_PERIOD_RAW 1000
#define TIMER3_PRESCALER (TIMER3_PRESCALER_RAW - 1)
#define TIMER3_PERIOD (TIMER3_PERIOD_RAW - 1)

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
