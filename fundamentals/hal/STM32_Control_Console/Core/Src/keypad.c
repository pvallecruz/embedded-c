/*
 * keypad.c
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */


#include "keypad.h"

#define KEYPAD_DEBOUNCE_MS 30U

typedef enum {
  KEYPAD_IDLE,
  KEYPAD_DEBOUNCE,
  KEYPAD_PRESSED,
  KEYPAD_RELEASE
} KeypadState;

// GPIO pins connected to the keypad rows.
static const uint16_t row_pins[4] = {
  GPIO_PIN_0,
  GPIO_PIN_1,
  GPIO_PIN_2,
  GPIO_PIN_3
};

// GPIO pins connected to the keypad columns.
static const uint16_t column_pins[4] = {
  GPIO_PIN_8,
  GPIO_PIN_9,
  GPIO_PIN_10,
  GPIO_PIN_11
};

// Physical keypad layout.
static const char key_map[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

char KeypadScan(void) {
  // Start with all rows inactive (HIGH).
  for (uint8_t row = 0; row < 4U; row++) {
    HAL_GPIO_WritePin(GPIOD, row_pins[row], GPIO_PIN_SET);
  }

  // Activate one row at a time.
  for (uint8_t row = 0; row < 4U; row++) {
    HAL_GPIO_WritePin(GPIOD, row_pins[row], GPIO_PIN_RESET);

    // Check each column for an active-low signal.
    for (uint8_t column = 0; column < 4U; column++) {
      if (HAL_GPIO_ReadPin(GPIOD, column_pins[column]) ==
          GPIO_PIN_RESET) {
        char key = key_map[row][column];

        // Restore the active row before returning.
        HAL_GPIO_WritePin(GPIOD, row_pins[row], GPIO_PIN_SET);

        return key;
      }
    }

    // Deactivate this row before scanning the next.
    HAL_GPIO_WritePin(GPIOD, row_pins[row], GPIO_PIN_SET);
  }

  // No key is currently pressed.
  return '\0';
}

char KeypadGetKey(void) {
  static KeypadState state = KEYPAD_IDLE;
  static char candidate_key = '\0';
  static uint32_t transition_time = 0U;

  char current_key = KeypadScan();
  uint32_t now = HAL_GetTick();

  switch (state) {
    case KEYPAD_IDLE:
      if (current_key != '\0') {
        candidate_key = current_key;
        transition_time = now;
        state = KEYPAD_DEBOUNCE;
      }
      break;

    case KEYPAD_DEBOUNCE:
      if (current_key != candidate_key) {
        // Restart validation if the observed key changes.
        candidate_key = current_key;
        transition_time = now;

        if (current_key == '\0') {
          state = KEYPAD_IDLE;
        }
      } else if ((uint32_t)(now - transition_time) >=
                 KEYPAD_DEBOUNCE_MS) {
        state = KEYPAD_PRESSED;
        return candidate_key;
      }
      break;

    case KEYPAD_PRESSED:
      if (current_key == '\0') {
        transition_time = now;
        state = KEYPAD_RELEASE;
      }
      break;

    case KEYPAD_RELEASE:
      if (current_key != '\0') {
        // A key is still detected; cancel release.
        state = KEYPAD_PRESSED;
      } else if ((uint32_t)(now - transition_time) >=
                 KEYPAD_DEBOUNCE_MS) {
        candidate_key = '\0';
        state = KEYPAD_IDLE;
      }
      break;

    default:
      state = KEYPAD_IDLE;
      candidate_key = '\0';
      break;
  }

  return '\0';
}

