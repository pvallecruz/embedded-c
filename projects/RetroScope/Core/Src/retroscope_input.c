/*
 * retroscope_input.c
 *
 *  Created on: Nov 22, 2026
 *      Author: pedrol.valle-cruz
 */

#include "retroscope_input.h"
#include "main.h"

#define BUTTON_DEBOUNCE_MS 30U

typedef struct {
  GPIO_PinState LastRaw;
  GPIO_PinState Stable;
  uint32_t ChangedTick;
  uint8_t Initialized;
} RetroScope_ButtonState;

static RetroScope_ButtonState RunStopButton = {0};
static RetroScope_ButtonState ModeButton = {0};
static RetroScope_ButtonState SelectButton = {0};
static RetroScope_ButtonState DecreaseButton = {0};
static RetroScope_ButtonState IncreaseButton = {0};

static uint8_t RetroScope_InputPollButton(RetroScope_ButtonState *State,
                                          GPIO_TypeDef *Port,
                                          uint16_t Pin) {
  GPIO_PinState Raw = HAL_GPIO_ReadPin(Port, Pin);
  uint32_t Now = HAL_GetTick();

  // Adopt the initial state without generating a press event.
  if (State->Initialized == 0U) {
    State->LastRaw = Raw;
    State->Stable = Raw;
    State->ChangedTick = Now;
    State->Initialized = 1U;
    return 0U;
  }

  // Restart the debounce interval whenever the raw reading changes.
  if (Raw != State->LastRaw) {
    State->LastRaw = Raw;
    State->ChangedTick = Now;
  }

  if (Raw != State->Stable &&
      (uint32_t)(Now - State->ChangedTick) >= BUTTON_DEBOUNCE_MS) {
    State->Stable = Raw;

    // Active-low button: report only the accepted press.
    if (State->Stable == GPIO_PIN_RESET) {
      return 1U;
    }
  }

  return 0U;
}

uint8_t RetroScope_InputPollRunStop(void) {
  return RetroScope_InputPollButton(&RunStopButton,
                                    RUN_STOP_BTN_GPIO_Port,
                                    RUN_STOP_BTN_Pin);
}

uint8_t RetroScope_InputPollMode(void) {
  return RetroScope_InputPollButton(&ModeButton,
                                    MODE_BTN_GPIO_Port,
                                    MODE_BTN_Pin);
}

uint8_t RetroScope_InputPollSelect(void) {
  return RetroScope_InputPollButton(&SelectButton,
                                    SELECT_BTN_GPIO_Port,
                                    SELECT_BTN_Pin);
}

uint8_t RetroScope_InputPollDecrease(void) {
  return RetroScope_InputPollButton(&DecreaseButton,
                                    DECREASE_BTN_GPIO_Port,
                                    DECREASE_BTN_Pin);
}

uint8_t RetroScope_InputPollIncrease(void) {
  return RetroScope_InputPollButton(&IncreaseButton,
                                    INCREASE_BTN_GPIO_Port,
                                    INCREASE_BTN_Pin);
}
