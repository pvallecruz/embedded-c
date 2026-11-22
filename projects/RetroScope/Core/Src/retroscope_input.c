/*
 * retroscope_input.c
 *
 *  Created on: Nov 22, 2026
 *      Author: pedrol.valle-cruz
 */

#include "retroscope_input.h"
#include "main.h"

#define BUTTON_DEBOUNCE_MS 30U

static GPIO_PinState RunStopLastRaw = GPIO_PIN_SET;
static GPIO_PinState RunStopStable = GPIO_PIN_SET;
static uint32_t RunStopChangedTick = 0U;
static uint8_t RunStopInitialized = 0U;

uint8_t RetroScope_InputPollRunStop(void) {
  GPIO_PinState Raw =
      HAL_GPIO_ReadPin(RUN_STOP_BTN_GPIO_Port, RUN_STOP_BTN_Pin);
  uint32_t Now = HAL_GetTick();

  // Adopt the initial state without generating a press event.
  if (RunStopInitialized == 0U) {
    RunStopLastRaw = Raw;
    RunStopStable = Raw;
    RunStopChangedTick = Now;
    RunStopInitialized = 1U;
    return 0U;
  }

  // Restart the debounce interval whenever the raw reading changes.
  if (Raw != RunStopLastRaw) {
    RunStopLastRaw = Raw;
    RunStopChangedTick = Now;
  }

  if (Raw != RunStopStable &&
      (uint32_t)(Now - RunStopChangedTick) >= BUTTON_DEBOUNCE_MS) {
    RunStopStable = Raw;

    // Active-low button: report only the accepted press.
    if (RunStopStable == GPIO_PIN_RESET) {
      return 1U;
    }
  }

  return 0U;
}
