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
#include "dma.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "retroscope_ui.h"
#include "st7796.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define FRAME_INTERVAL_MS 33U
#define PHASE_INCREMENT 0.05f
#define TWO_PI 6.2831853f
#define ADC_SAMPLES_PER_HALF 802U
#define ADC_BUFFER_SIZE (ADC_SAMPLES_PER_HALF * 2U)
#define ADC_DISPLAY_SAMPLES 401U
#define ADC_TRIGGER_LEVEL 2048U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint32_t elapsed_time = 0;
volatile uint32_t apb2_clock = 0;
volatile uint32_t measurement_count = 0;
volatile uint32_t cpu_clock = 0;
static uint32_t LastFrameTick = 0U;
static float WaveformPhase = 0.0f;
volatile uint32_t FrameCycles = 0U;
volatile float FrameTimeMs = 0.0f;
volatile uint32_t AdcRawValue = 0U;
volatile float AdcVoltage = 0.0f;

volatile uint32_t drawing_cycles = 0;
volatile float drawing_ms = 0;
volatile uint32_t start_cycles = 0;
volatile float MaxFrameTimeMs = 0.0f;
volatile float MinFrameTimeMs = 1000000.0f;
volatile float SpiTransferMs = 0.0f;
volatile float ActualFPS = 0.0f;
static uint32_t PreviousFrameStart = 0U;
uint16_t AdcBuffer[ADC_BUFFER_SIZE];
uint16_t AdcSnapshot[ADC_DISPLAY_SAMPLES];
uint16_t AdcDisplayBuffer[ADC_DISPLAY_SAMPLES];

volatile float ActualSampleRate = 0.0f;

static uint32_t LastSampleRateTick = 0U;
static uint32_t PreviousBufferCount = 0U;
volatile uint32_t AdcHalfCompleteCount = 0U;
volatile uint32_t AdcBufferCompleteCount = 0U;
volatile uint16_t AdcLatestSample = 0U;
volatile uint32_t AdcSnapshotCount = 0U;

static uint32_t LastDisplayedSnapshot = 0U;
volatile uint32_t DisplaySnapshotCount = 0U;
volatile uint16_t AdcMinValue = 4095U;
volatile uint16_t AdcMaxValue = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

extern volatile uint32_t SpiTransferCycles;
extern volatile uint16_t StripsDrawn;

static void RetroScope_EnableCycleCounter(void) {
  // Enable trace functionality
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // Reset the cycle counter
  DWT->CYCCNT = 0U;

  // Enable cycle counting
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint8_t RetroScope_GetAdcSnapshot(void) {
  uint32_t primask = __get_PRIMASK();

  __disable_irq();

  if (AdcSnapshotCount == LastDisplayedSnapshot) {
    __set_PRIMASK(primask);
    return 0U;
  }

  for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
    AdcDisplayBuffer[i] = AdcSnapshot[i];
  }

  LastDisplayedSnapshot = AdcSnapshotCount;

  __set_PRIMASK(primask);

  DisplaySnapshotCount++;

  return 1U;
}

static void RetroScope_MeasureAdcRange(void) {
  uint16_t min = 4095U;
  uint16_t max = 0U;

  for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
    uint16_t sample = AdcDisplayBuffer[i];

    if (sample < min) {
      min = sample;
    }

    if (sample > max) {
      max = sample;
    }
  }

  AdcMinValue = min;
  AdcMaxValue = max;
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  ST7796_Init();

  RetroScope_DrawLayout();
  RetroScope_DrawGrid();
  RetroScope_DrawLabels();

  RetroScope_EnableCycleCounter();

  RetroScope_GenerateWaveform(WaveformPhase);
  RetroScope_DrawWaveformFrame();

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)AdcBuffer, ADC_BUFFER_SIZE) !=
      HAL_OK) {
    Error_Handler();
  }

  if (HAL_TIM_Base_Start(&htim2) != HAL_OK) {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK) {
    Error_Handler();
  }

  LastFrameTick = HAL_GetTick();

  /* start_cycles = DWT->CYCCNT;

  RetroScope_DrawWaveformAdaptive();

  drawing_cycles = DWT->CYCCNT - start_cycles;

  drawing_ms =
      (float)drawing_cycles * 1000.0f
      / (float)HAL_RCC_GetHCLKFreq();*/

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t CurrentTick = HAL_GetTick();

    if ((uint32_t)(CurrentTick - LastFrameTick) >= FRAME_INTERVAL_MS) {
      if (PreviousFrameStart != 0U) {
        uint32_t elapsed = CurrentTick - PreviousFrameStart;

        if (elapsed > 0U) {
          ActualFPS = 1000.0f / (float)elapsed;
        }
      }

      PreviousFrameStart = CurrentTick;

      LastFrameTick = CurrentTick;

      // Advance the waveform phase
      WaveformPhase += PHASE_INCREMENT;

      if (WaveformPhase >= TWO_PI) {
        WaveformPhase -= TWO_PI;
      }

      SpiTransferCycles = 0U;
      uint32_t StartCycles = DWT->CYCCNT;

      // Generate and draw the next frame
      if (RetroScope_GetAdcSnapshot()) {
        RetroScope_MeasureAdcRange();

        RetroScope_LoadADCWaveform(AdcDisplayBuffer);
        RetroScope_DrawWaveformFrame();
      }

      FrameCycles = DWT->CYCCNT - StartCycles;

      FrameTimeMs = (float)FrameCycles * 1000.0f / (float)HAL_RCC_GetHCLKFreq();

      if (FrameTimeMs > MaxFrameTimeMs) {
        MaxFrameTimeMs = FrameTimeMs;
      }

      if (FrameTimeMs < MinFrameTimeMs) {
        MinFrameTimeMs = FrameTimeMs;
      }

      SpiTransferMs =
          (float)SpiTransferCycles * 1000.0f / (float)HAL_RCC_GetHCLKFreq();
    }

    uint32_t SampleRateTick = HAL_GetTick();

    if ((uint32_t)(SampleRateTick - LastSampleRateTick) >= 1000U) {
      uint32_t elapsed = SampleRateTick - LastSampleRateTick;

      uint32_t completed = AdcBufferCompleteCount - PreviousBufferCount;

      ActualSampleRate =
          ((float)completed * ADC_BUFFER_SIZE * 1000.0f) / (float)elapsed;

      PreviousBufferCount = AdcBufferCompleteCount;
      LastSampleRateTick = SampleRateTick;
    }
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

static void RetroScope_CopyAdcSamples(uint16_t start) {
  uint16_t triggerIndex = 0U;
  uint8_t triggerFound = 0U;

  // Find a rising edge with enough samples remaining.
  for (uint16_t i = 1U; i <= ADC_SAMPLES_PER_HALF - ADC_DISPLAY_SAMPLES; i++) {
    uint16_t previous = AdcBuffer[start + i - 1U];
    uint16_t current = AdcBuffer[start + i];

    if (previous < ADC_TRIGGER_LEVEL && current >= ADC_TRIGGER_LEVEL) {
      triggerIndex = i;
      triggerFound = 1U;
      break;
    }
  }

  // No trigger: display the beginning of the capture.
  if (triggerFound == 0U) {
    triggerIndex = 0U;
  }

  // Copy 401 samples starting at the trigger.
  for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
    AdcSnapshot[i] = AdcBuffer[start + triggerIndex + i];
  }

  AdcLatestSample = AdcSnapshot[ADC_DISPLAY_SAMPLES - 1U];
  AdcSnapshotCount++;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc->Instance == ADC1) {
    RetroScope_CopyAdcSamples(0U);
    AdcHalfCompleteCount++;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc->Instance == ADC1) {
    RetroScope_CopyAdcSamples(ADC_SAMPLES_PER_HALF);
    AdcBufferCompleteCount++;
  }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t* file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
