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
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "retroscope_ui.h"
#include "st7796.h"
#include "retroscope_input.h"

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
#define ADC_TRIGGER_HYSTERESIS 64U
#define ADC_TRIGGER_EDGE_RISING 0U
#define ADC_TRIGGER_EDGE_FALLING 1U
#define ADC_TRIGGER_MODE_AUTO 0U
#define ADC_TRIGGER_MODE_NORMAL 1U
#define ADC_TRIGGER_MODE_SINGLE 2U
#define SINGLE_STATE_IDLE 0U
#define SINGLE_STATE_ARMED 1U
#define SINGLE_STATE_CAPTURED 2U
#define SINGLE_STATE_RENDERING 3U
// Nominal sampling rate for the current fixed TIM2 configuration.
#define ADC_SAMPLE_RATE_HZ 10000U
// Frequency detection thresholds in raw ADC counts.
#define ADC_FREQUENCY_LOW_LEVEL 1984U
#define ADC_FREQUENCY_HIGH_LEVEL 2112U
// Assumed ADC reference voltage; not yet calibrated.
#define ADC_REFERENCE_VOLTS 3.3f
#define ADC_MAX_COUNT 4095U

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
static uint32_t LastFrequencyDisplayTick = 0U;
volatile float AdcMinVolts = 0.0f;
volatile float AdcMaxVolts = 0.0f;
volatile float AdcPeakToPeakVolts = 0.0f;
// Display control: 0 stops updates; nonzero runs.
volatile uint32_t RequestedRun = 1U;

// Applied only between complete waveform frames.
static uint8_t DisplayRunning = 1U;
// Requested horizontal scale, in microseconds per division.
volatile uint32_t RequestedTimebaseUs = RETROSCOPE_TIMEBASE_4_MS;
// Requested vertical display mode.
volatile uint32_t RequestedVerticalScale = RETROSCOPE_VERTICAL_FULL_RANGE;
// Requested waveform trigger edge; rising is the default.
volatile uint32_t RequestedTriggerEdge = ADC_TRIGGER_EDGE_RISING;
// Requested trigger center level, in raw ADC counts.
volatile uint32_t RequestedTriggerLevel = ADC_TRIGGER_LEVEL;
static uint32_t SnapshotTriggerEdge = ADC_TRIGGER_EDGE_RISING;
// Requested trigger mode; Auto allows free-running fallback captures.
volatile uint32_t RequestedTriggerMode = ADC_TRIGGER_MODE_AUTO;
// Write 1 to request a new Single capture; main clears it when handled.
volatile uint32_t RequestedSingleArm = 0U;
static uint32_t SnapshotTriggerLevel = ADC_TRIGGER_LEVEL;
// 1: a trigger was found. 0: this snapshot uses the fallback window.
static uint8_t SnapshotTriggered = 0U;
static float SnapshotFrequencyHz = 0.0f;
static uint8_t SnapshotFrequencyValid = 0U;
static float DisplayFrequencyHz = 0.0f;
static uint8_t DisplayFrequencyValid = 0U;
static uint32_t ActiveTriggerMode = ADC_TRIGGER_MODE_AUTO;
static uint8_t TriggerWaiting = 0U;
// Single capture progresses from armed to captured to rendering.
static uint8_t SingleState = SINGLE_STATE_IDLE;



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

  DisplayFrequencyHz = SnapshotFrequencyHz;
  DisplayFrequencyValid = SnapshotFrequencyValid;
  LastDisplayedSnapshot = AdcSnapshotCount;

  DisplaySnapshotCount++;

  return 1U;
}

static void RetroScope_MeasureAdcRange(void) {
  uint16_t min = ADC_MAX_COUNT;
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

  const float VoltsPerCount = ADC_REFERENCE_VOLTS / (float)ADC_MAX_COUNT;

  AdcMinVolts = (float)min * VoltsPerCount;
  AdcMaxVolts = (float)max * VoltsPerCount;
  AdcPeakToPeakVolts = (float)(max - min) * VoltsPerCount;
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

    if (RetroScope_InputPollRunStop() != 0U) {
      if (RequestedTriggerMode == ADC_TRIGGER_MODE_SINGLE) {
        if (SingleState == SINGLE_STATE_IDLE) {
          RequestedSingleArm = 1U;
        } else {
          RequestedSingleArm = 0U;
          RequestedRun = 0U;
        }
      } else {
        RequestedRun = (RequestedRun != 0U) ? 0U : 1U;
      }
    }

    // Stop or leaving Single mode cancels an unfinished Single operation.
    if (SingleState != SINGLE_STATE_IDLE &&
        (RequestedRun == 0U ||
         RequestedTriggerMode != ADC_TRIGGER_MODE_SINGLE)) {
      SingleState = SINGLE_STATE_IDLE;
      TriggerWaiting = 0U;

      // Discard any accepted snapshot that has not started displaying.
      LastDisplayedSnapshot = AdcSnapshotCount;
    }
    if (AdcCaptureReady != 0U) {
      __DMB();

      RetroScope_MeasureFrequency(AdcCaptureBuffer);
      RetroScope_ProcessAdcCapture(AdcCaptureBuffer);

      __DMB();
      // Release only after all reads of the capture have completed.
      AdcCaptureReady = 0U;
    }

    uint32_t CurrentTick = HAL_GetTick();

    // Apply Run/Stop only after the current frame is complete.
    if (RetroScope_IsFrameActive() == 0U) {
	  // Accept an arm request only when Single is idle.
	  if (RequestedSingleArm != 0U) {
		RequestedSingleArm = 0U;

		if (RequestedTriggerMode == ADC_TRIGGER_MODE_SINGLE &&
			SingleState == SINGLE_STATE_IDLE) {
	      // Skip any older snapshot still pending for display.
		  LastDisplayedSnapshot = AdcSnapshotCount;

		  SingleState = SINGLE_STATE_ARMED;
		  TriggerWaiting = 1U;
		  RequestedRun = 1U;
		  PreviousFrameStart = 0U;
		 }
	  }
      uint8_t RunRequested = (RequestedRun != 0U) ? 1U : 0U;

      if (RunRequested != DisplayRunning) {
        DisplayRunning = RunRequested;

        // Exclude time spent stopped from the next FPS calculation.
        PreviousFrameStart = 0U;
      }

      RetroScope_UpdateRunStatus(DisplayRunning);

      if (DisplayRunning != 0U) {
        RetroScope_UpdateTriggerMode(ActiveTriggerMode);
        if (TriggerWaiting != 0U) {
          RetroScope_UpdateTriggerStatus(0U, 1U);
        }
      }
    }

    if (DisplayRunning != 0U &&
        RetroScope_IsFrameActive() == 0U &&
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

        uint32_t TimebaseUs = RequestedTimebaseUs;

        if (TimebaseUs != RETROSCOPE_TIMEBASE_2_MS) {
          TimebaseUs = RETROSCOPE_TIMEBASE_4_MS;
        }

        uint32_t VerticalScale = RequestedVerticalScale;

        if (VerticalScale != RETROSCOPE_VERTICAL_1_V_DIV) {
          VerticalScale = RETROSCOPE_VERTICAL_FULL_RANGE;
        }

        RetroScope_MeasureAdcRange();
        RetroScope_LoadAdcWaveform(AdcDisplayBuffer, TimebaseUs, VerticalScale,
                                   ADC_REFERENCE_VOLTS);
        RetroScope_UpdateTimebase(TimebaseUs);
        RetroScope_UpdateVerticalScale(VerticalScale);
        RetroScope_UpdateTriggerEdge(SnapshotTriggerEdge ==
                                     ADC_TRIGGER_EDGE_FALLING);
        float TriggerVolts =
            ((float)SnapshotTriggerLevel / (float)ADC_MAX_COUNT) *
            ADC_REFERENCE_VOLTS;
        uint32_t TriggerCentivolts = (uint32_t)(TriggerVolts * 100.0f + 0.5f);

        RetroScope_UpdateTriggerLevel(TriggerCentivolts);
        RetroScope_UpdateTriggerMarker(SnapshotTriggerLevel,
                                        VerticalScale,
                                        ADC_REFERENCE_VOLTS);
        RetroScope_UpdateTriggerStatus(SnapshotTriggered, TriggerWaiting);
        // Track the frame that contains the accepted Single capture.
        if (ActiveTriggerMode == ADC_TRIGGER_MODE_SINGLE &&
            SingleState == SINGLE_STATE_CAPTURED) {
          SingleState = SINGLE_STATE_RENDERING;
        }
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

    uint32_t FrequencyDisplayTick = HAL_GetTick();

    if (DisplayRunning != 0U &&
        RetroScope_IsFrameActive() == 0U &&
        ((uint32_t)(FrequencyDisplayTick - LastFrequencyDisplayTick) >= 250U ||
         SingleState == SINGLE_STATE_RENDERING)) {
	  float FrequencyHz = DisplayFrequencyHz;
	  uint8_t Valid = (DisplayFrequencyValid != 0U && FrequencyHz > 0.0f &&
					   FrequencyHz <= (float)ADC_SAMPLE_RATE_HZ / 2.0f);

      uint32_t RoundedHz = 0U;

      if (Valid != 0U) {
        RoundedHz = (uint32_t)(FrequencyHz + 0.5f);
      }

      RetroScope_UpdateFrequency(RoundedHz, Valid);
      float PeakToPeakVolts = AdcPeakToPeakVolts;
      uint8_t VppValid =
          (DisplaySnapshotCount != 0U && PeakToPeakVolts >= 0.0f &&
           PeakToPeakVolts <= ADC_REFERENCE_VOLTS);

      uint32_t Centivolts = 0U;

      if (VppValid != 0U) {
        Centivolts = (uint32_t)(PeakToPeakVolts * 100.0f + 0.5f);
      }

      RetroScope_UpdateVpp(Centivolts, VppValid);
      LastFrequencyDisplayTick = FrequencyDisplayTick;
      // Hold the completed Single frame and its measurements.
      if (SingleState == SINGLE_STATE_RENDERING) {
        SingleState = SINGLE_STATE_IDLE;
        RequestedRun = 0U;
        DisplayRunning = 0U;
        PreviousFrameStart = 0U;
        RetroScope_UpdateRunStatus(DisplayRunning);
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
void SystemClock_Config(void)
{
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
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
  uint8_t TriggerArmed = 0U;

  // Read once; unsupported modes use Auto.
  uint32_t TriggerMode = RequestedTriggerMode;
  if (TriggerMode != ADC_TRIGGER_MODE_NORMAL &&
      TriggerMode != ADC_TRIGGER_MODE_SINGLE) {
    TriggerMode = ADC_TRIGGER_MODE_AUTO;
  }

  // Read once so the selected edge stays consistent during this capture.
  uint32_t TriggerEdge = RequestedTriggerEdge;
  if (TriggerEdge != ADC_TRIGGER_EDGE_FALLING) {
    TriggerEdge = ADC_TRIGGER_EDGE_RISING;
  }

  // Read once and keep both hysteresis thresholds within the ADC range.
  uint32_t TriggerLevel = RequestedTriggerLevel;

  if (TriggerLevel < ADC_TRIGGER_HYSTERESIS) {
    TriggerLevel = ADC_TRIGGER_HYSTERESIS;
  } else if (TriggerLevel > ADC_MAX_COUNT - ADC_TRIGGER_HYSTERESIS) {
    TriggerLevel = ADC_MAX_COUNT - ADC_TRIGGER_HYSTERESIS;
  }

  uint32_t TriggerLowLevel = TriggerLevel - ADC_TRIGGER_HYSTERESIS;
  uint32_t TriggerHighLevel = TriggerLevel + ADC_TRIGGER_HYSTERESIS;

  // Keep enough samples remaining for the complete display window.
  for (uint16_t i = 0U; i <= ADC_SAMPLES_PER_HALF - ADC_DISPLAY_SAMPLES; i++) {
    uint16_t Sample = Samples[i];

    if (TriggerEdge == ADC_TRIGGER_EDGE_FALLING) {
      if (Sample >= TriggerHighLevel) {
        TriggerArmed = 1U;
      } else if (TriggerArmed != 0U && Sample <= TriggerLowLevel) {
        triggerFound = 1U;
      }
    } else {
      if (Sample <= TriggerLowLevel) {
        TriggerArmed = 1U;
      } else if (TriggerArmed != 0U && Sample >= TriggerHighLevel) {
        triggerFound = 1U;
      }
    }

    if (triggerFound != 0U) {
      triggerIndex = i;
      break;
    }
  }

  // Acquisition status updates even when Normal holds the last snapshot.
  ActiveTriggerMode = TriggerMode;
  TriggerWaiting =
      (triggerFound == 0U &&
       (TriggerMode == ADC_TRIGGER_MODE_NORMAL ||
        (TriggerMode == ADC_TRIGGER_MODE_SINGLE &&
         SingleState == SINGLE_STATE_ARMED)))
          ? 1U
          : 0U;

  // Single accepts a triggered capture only while explicitly armed.
  uint8_t PublishCapture =
      (TriggerMode == ADC_TRIGGER_MODE_AUTO ||
       (TriggerMode == ADC_TRIGGER_MODE_NORMAL && triggerFound != 0U) ||
       (TriggerMode == ADC_TRIGGER_MODE_SINGLE &&
        SingleState == SINGLE_STATE_ARMED && triggerFound != 0U));

  if (PublishCapture != 0U) {
    if (triggerFound == 0U) {
      triggerIndex = 0U;
    }

    // Copy the complete display window.
    for (uint16_t i = 0U; i < ADC_DISPLAY_SAMPLES; i++) {
      AdcSnapshot[i] = Samples[triggerIndex + i];
    }

    AdcLatestSample = AdcSnapshot[ADC_DISPLAY_SAMPLES - 1U];
    SnapshotTriggerEdge = TriggerEdge;
    SnapshotTriggerLevel = TriggerLevel;
    SnapshotTriggered = triggerFound;
    SnapshotFrequencyHz = AdcFrequencyHz;
    SnapshotFrequencyValid = (AdcFrequencyValid != 0U) ? 1U : 0U;
    AdcSnapshotCount++;

    if (TriggerMode == ADC_TRIGGER_MODE_SINGLE) {
      SingleState = SINGLE_STATE_CAPTURED;
    }
  }

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
void Error_Handler(void)
{
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
