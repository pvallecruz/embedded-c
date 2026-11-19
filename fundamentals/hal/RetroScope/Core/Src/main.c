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
#define ADC_SAMPLES_PER_HALF 802U
#define ADC_BUFFER_SIZE (ADC_SAMPLES_PER_HALF * 2U)
#define ADC_DISPLAY_SAMPLES RETROSCOPE_WAVEFORM_SAMPLES
#define ADC_TRIGGER_LEVEL 2048U
// Nominal sampling rate for the current fixed TIM2 configuration.
#define ADC_SAMPLE_RATE_HZ 10000U
// Frequency detection thresholds in raw ADC counts.
#define ADC_FREQUENCY_LOW_LEVEL 1984U
#define ADC_FREQUENCY_HIGH_LEVEL 2112U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint32_t LastFrameTick = 0U;
static uint32_t FrameStartCycles = 0U;
volatile uint32_t FrameCycles = 0U;
volatile float FrameTimeMs = 0.0f;

volatile float MaxFrameTimeMs = 0.0f;
volatile float MinFrameTimeMs = 1000000.0f;
volatile float SpiTransferMs = 0.0f;
volatile float ActualFPS = 0.0f;
static uint32_t PreviousFrameStart = 0U;
// DMA writes AdcBuffer; the ISR publishes a copy into AdcCaptureBuffer.
uint16_t AdcBuffer[ADC_BUFFER_SIZE];
uint16_t AdcSnapshot[ADC_DISPLAY_SAMPLES];
uint16_t AdcDisplayBuffer[ADC_DISPLAY_SAMPLES];
uint16_t AdcCaptureBuffer[ADC_SAMPLES_PER_HALF];
// 0: ISR may write. 1: main owns the capture until processing finishes.
volatile uint32_t AdcCaptureReady = 0U;
volatile uint32_t AdcDroppedCaptureCount = 0U;
volatile uint32_t AdcUnsafeCaptureCount = 0U;

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
// Main-loop trigger processing time, including any interrupt preemption.
volatile uint32_t AdcProcessingCycles = 0U;
volatile uint32_t MaxAdcProcessingCycles = 0U;
// Successful DMA-half copy and publication time; excludes skipped captures.
volatile uint32_t AdcCaptureCopyCycles = 0U;
volatile uint32_t MaxAdcCaptureCopyCycles = 0U;
volatile float AdcFrequencyHz = 0.0f;
volatile uint32_t AdcFrequencyValid = 0U;
volatile uint32_t AdcFrequencyPeriodCount = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void RetroScope_ProcessAdcCapture(const uint16_t* Samples);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void RetroScope_MeasureFrequency(const uint16_t* Samples) {
  uint16_t FirstEdge = 0U;
  uint16_t LastEdge = 0U;
  uint16_t EdgeCount = 0U;
  uint8_t EdgeArmed = 0U;

  AdcFrequencyValid = 0U;
  AdcFrequencyHz = 0.0f;
  AdcFrequencyPeriodCount = 0U;

  for (uint16_t i = 0U; i < ADC_SAMPLES_PER_HALF; i++) {
    uint16_t Sample = Samples[i];

    if (Sample <= ADC_FREQUENCY_LOW_LEVEL) {
      EdgeArmed = 1U;
    } else if (EdgeArmed != 0U && Sample >= ADC_FREQUENCY_HIGH_LEVEL) {
      if (EdgeCount == 0U) {
        FirstEdge = i;
      }

      LastEdge = i;
      EdgeCount++;
      EdgeArmed = 0U;
    }
  }

  if (EdgeCount < 2U) {
    return;
  }

  uint32_t PeriodCount = EdgeCount - 1U;
  uint32_t SampleSpan = LastEdge - FirstEdge;

  AdcFrequencyHz =
      ((float)ADC_SAMPLE_RATE_HZ * (float)PeriodCount) / (float)SampleSpan;

  AdcFrequencyPeriodCount = PeriodCount;
  AdcFrequencyValid = 1U;
}

static void RetroScope_EnableCycleCounter(void) {
  // Enable trace functionality
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

  // Reset the cycle counter
  DWT->CYCCNT = 0U;

  // Enable cycle counting
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint8_t RetroScope_GetAdcSnapshot(void) {
  // Snapshot production and display copying both run in the main loop.
  if (AdcSnapshotCount == LastDisplayedSnapshot) {
    return 0U;
  }

  for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
    AdcDisplayBuffer[i] = AdcSnapshot[i];
  }

  LastDisplayedSnapshot = AdcSnapshotCount;

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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (AdcCaptureReady != 0U) {
      __DMB();

      RetroScope_ProcessAdcCapture(AdcCaptureBuffer);
      RetroScope_MeasureFrequency(AdcCaptureBuffer);

      __DMB();
      // Release only after all reads of the capture have completed.
      AdcCaptureReady = 0U;
    }

    uint32_t CurrentTick = HAL_GetTick();

    if (RetroScope_IsFrameActive() == 0U &&
        (uint32_t)(CurrentTick - LastFrameTick) >= FRAME_INTERVAL_MS) {
      if (RetroScope_GetAdcSnapshot()) {
        if (PreviousFrameStart != 0U) {
          uint32_t Elapsed = CurrentTick - PreviousFrameStart;

          if (Elapsed > 0U) {
            ActualFPS = 1000.0f / (float)Elapsed;
          }
        }

        PreviousFrameStart = CurrentTick;
        LastFrameTick = CurrentTick;

        SpiTransferCycles = 0U;
        FrameStartCycles = DWT->CYCCNT;

        RetroScope_MeasureAdcRange();
        RetroScope_LoadAdcWaveform(AdcDisplayBuffer);
        RetroScope_BeginFrame();
      }
    }

    if (RetroScope_IsFrameActive() != 0U) {
      RetroScope_RenderStep();

      if (RetroScope_IsFrameActive() == 0U) {
        FrameCycles = DWT->CYCCNT - FrameStartCycles;
        FrameTimeMs =
            (float)FrameCycles * 1000.0f / (float)HAL_RCC_GetHCLKFreq();

        if (FrameTimeMs > MaxFrameTimeMs) {
          MaxFrameTimeMs = FrameTimeMs;
        }

        if (FrameTimeMs < MinFrameTimeMs) {
          MinFrameTimeMs = FrameTimeMs;
        }

        SpiTransferMs =
            (float)SpiTransferCycles * 1000.0f / (float)HAL_RCC_GetHCLKFreq();
      }
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

// ISR only: copy before DMA returns to this half (nominally 80.2 ms).
// The occupied-buffer counter does not detect a late DMA interrupt.
static void RetroScope_PublishAdcCapture(const volatile uint16_t* Samples) {
  if (AdcCaptureReady != 0U) {
    AdcDroppedCaptureCount++;
    return;
  }

  uint32_t Remaining = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
  uint8_t SourceSafe = 0U;

  if (Samples == &AdcBuffer[0U]) {
    SourceSafe = (Remaining > 0U && Remaining <= ADC_SAMPLES_PER_HALF);
  } else if (Samples == &AdcBuffer[ADC_SAMPLES_PER_HALF]) {
    SourceSafe =
        (Remaining > ADC_SAMPLES_PER_HALF && Remaining <= ADC_BUFFER_SIZE);
  }

  if (SourceSafe == 0U) {
    AdcUnsafeCaptureCount++;
    return;
  }

  uint32_t StartCycles = DWT->CYCCNT;

  __DMB();

  for (uint16_t i = 0U; i < ADC_SAMPLES_PER_HALF; i++) {
    AdcCaptureBuffer[i] = Samples[i];
  }

  __DMB();

  uint32_t RemainingAfter = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
  uint32_t BoundaryFlag;

  if (Samples == &AdcBuffer[0U]) {
    SourceSafe =
        (RemainingAfter > 0U && RemainingAfter <= ADC_SAMPLES_PER_HALF);
    BoundaryFlag = DMA_FLAG_TCIF0_4;
  } else {
    SourceSafe = (RemainingAfter > ADC_SAMPLES_PER_HALF &&
                  RemainingAfter <= ADC_BUFFER_SIZE);
    BoundaryFlag = DMA_FLAG_HTIF0_4;
  }

  if (SourceSafe == 0U ||
      __HAL_DMA_GET_FLAG(hadc1.DMA_Handle, BoundaryFlag) != RESET) {
    AdcUnsafeCaptureCount++;
    return;
  }

  __DMB();
  // Publish only after the copy passes validation.
  AdcCaptureReady = 1U;

  uint32_t ElapsedCycles = DWT->CYCCNT - StartCycles;

  AdcCaptureCopyCycles = ElapsedCycles;

  if (ElapsedCycles > MaxAdcCaptureCopyCycles) {
    MaxAdcCaptureCopyCycles = ElapsedCycles;
  }
}

static void RetroScope_ProcessAdcCapture(const uint16_t* Samples) {
  uint32_t StartCycles = DWT->CYCCNT;
  uint16_t triggerIndex = 0U;
  uint8_t triggerFound = 0U;

  // Find a rising edge with enough samples remaining.
  for (uint16_t i = 1U; i <= ADC_SAMPLES_PER_HALF - ADC_DISPLAY_SAMPLES; i++) {
    uint16_t previous = Samples[i - 1U];
    uint16_t current = Samples[i];

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

  // Copy the display window starting at the trigger.
  for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
    AdcSnapshot[i] = Samples[triggerIndex + i];
  }

  AdcLatestSample = AdcSnapshot[ADC_DISPLAY_SAMPLES - 1U];
  AdcSnapshotCount++;

  uint32_t ElapsedCycles = DWT->CYCCNT - StartCycles;

  AdcProcessingCycles = ElapsedCycles;

  if (ElapsedCycles > MaxAdcProcessingCycles) {
    MaxAdcProcessingCycles = ElapsedCycles;
  }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc->Instance == ADC1) {
    RetroScope_PublishAdcCapture(&AdcBuffer[0U]);
    AdcHalfCompleteCount++;
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc->Instance == ADC1) {
    RetroScope_PublishAdcCapture(&AdcBuffer[ADC_SAMPLES_PER_HALF]);
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
