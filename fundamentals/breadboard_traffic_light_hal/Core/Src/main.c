/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @author         : Pedro Valle-Cruz
  * @brief          : Nonblocking HAL traffic light with debounced pedestrian
  *                   requests and a steady/blinking crossing indicator.
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

#include "main.h"

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
typedef enum {
  TRAFFIC_RED,
  TRAFFIC_GREEN,
  TRAFFIC_YELLOW
} traffic_state_t;
/* USER CODE END PTD */

/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* USER CODE BEGIN PV */
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  traffic_state_t state = TRAFFIC_RED;
  uint32_t state_started = HAL_GetTick();
  /* Seed from the input: holding at startup is not a new press. */
  GPIO_PinState button_raw = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_10);
  GPIO_PinState button_stable = button_raw;
  uint32_t button_changed_at = HAL_GetTick();
  unsigned int pedestrian_requested = 0U;
  uint32_t pedestrian_last_toggle = HAL_GetTick();

  HAL_GPIO_WritePin(GPIOE, RED_LED_Pin, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* Unsigned elapsed-time subtraction tolerates tick wraparound. */
    uint32_t now = HAL_GetTick();
    GPIO_PinState button_sample = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_10);

    /* Each raw edge restarts the 20 ms stability interval. */
    if (button_sample != button_raw) {
      button_raw = button_sample;
      button_changed_at = now;
    }

    if ((button_raw != button_stable) &&
        ((now - button_changed_at) >= 20U)) {
      button_stable = button_raw;
      /* Accept only a new press during green; repeated requests coalesce. */
      if ((button_stable == GPIO_PIN_RESET) && (state == TRAFFIC_GREEN)) {
        pedestrian_requested = 1U;
      }
    }

    switch (state) {
    case TRAFFIC_RED:
      if ((now - state_started) >= 10000U) {
        HAL_GPIO_WritePin(GPIOE, RED_LED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, PEDESTRIAN_LED_Pin, GPIO_PIN_RESET);
        /* Finish the crossing before enabling green. */
        pedestrian_requested = 0U;
        HAL_GPIO_WritePin(GPIOE, GREEN_LED_Pin, GPIO_PIN_SET);
        state = TRAFFIC_GREEN;
        state_started = now;
      } else if ((pedestrian_requested == 1U) &&
                 ((now - state_started) >= 8000U)) {
        /* First toggle at 8 s of red; subsequent toggles every 250 ms. */
        if ((now - pedestrian_last_toggle) >= 250U) {
          HAL_GPIO_TogglePin(GPIOE, PEDESTRIAN_LED_Pin);
          pedestrian_last_toggle = now;
        }
      }
      break;

    case TRAFFIC_GREEN:
      /* A request shortens green, but never below two seconds total. */
      if (((now - state_started) >= 10000U) ||
          ((pedestrian_requested == 1U) &&
           ((now - state_started) >= 2000U))) {
        HAL_GPIO_WritePin(GPIOE, GREEN_LED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, YELLOW_LED_Pin, GPIO_PIN_SET);
        state = TRAFFIC_YELLOW;
        state_started = now;
      }
      break;

    case TRAFFIC_YELLOW:
      if ((now - state_started) >= 5000U) {
        HAL_GPIO_WritePin(GPIOE, YELLOW_LED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOE, RED_LED_Pin, GPIO_PIN_SET);
        if (pedestrian_requested == 1U) {
          HAL_GPIO_WritePin(GPIOE, PEDESTRIAN_LED_Pin, GPIO_PIN_SET);
        }
        state = TRAFFIC_RED;
        state_started = now;
        pedestrian_last_toggle = now;
      }
      break;
    }
  }
  /* USER CODE END 3 */
}

/* Nominal 16 MHz HSI; AHB and APB clocks remain undivided. */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Active-high LED outputs and an active-low PE10 button with pull-up. */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOE, RED_LED_Pin|YELLOW_LED_Pin|GREEN_LED_Pin|PEDESTRIAN_LED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = RED_LED_Pin|YELLOW_LED_Pin|GREEN_LED_Pin|PEDESTRIAN_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif
