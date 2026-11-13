/*
 * console.c
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#include <stdio.h>
#include <string.h>

#include "console.h"
#include "keypad.h"
#include "lcd.h"
#include "led.h"
#include "temperature.h"
#include "buzzer.h"
#include "countdown.h"

static HAL_StatusTypeDef ConsoleUpdateLedState(LedId led);
static HAL_StatusTypeDef ConsoleUpdateTemperature(void);

static ConsoleState current_state = CONSOLE_MAIN_MENU;
static uint32_t status_displayed_second = 0U;
static uint32_t timer_entry_seconds = 0U;

static HAL_StatusTypeDef ConsoleShowMainMenu(void) {
  HAL_StatusTypeDef status = LcdClear();

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(0, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("STM32 CONTROL");

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(1, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("1. LED Control");

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(2, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("2. System Status");

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(3, 0);
  if (status != HAL_OK) {
    return status;
  }

  return LcdPrint("3. Timer Settings");
}

static HAL_StatusTypeDef ConsoleUpdateTimerDisplay(uint8_t force) {
  static char previous[3][21] = {{0}};
  char lines[3][21];
  char value[32];

  CountdownState state = CountdownGetState();
  const char *instructions;
  const char *navigation;

  if (state == COUNTDOWN_RUNNING) {
    snprintf(value, sizeof(value), "Remaining: %lu s",
             (unsigned long)CountdownGetRemainingSeconds());

    instructions = "# Cancel";
    navigation = "* Back";
  } else if (state == COUNTDOWN_COMPLETE) {
    snprintf(value, sizeof(value), "Timer complete");

    instructions = BuzzerAlarmIsActive() ? "# Silence alarm" : "# Reset";
    navigation = "* Back";
  } else {
    snprintf(value, sizeof(value), "Seconds: %lu",
             (unsigned long)timer_entry_seconds);

    instructions = timer_entry_seconds == 0U
                       ? "Enter 1-999 seconds"
                       : "# Start";
    navigation = "D Clear   * Back";
  }

  snprintf(lines[0], sizeof(lines[0]), "%-20.20s", value);
  snprintf(lines[1], sizeof(lines[1]), "%-20.20s", instructions);
  snprintf(lines[2], sizeof(lines[2]), "%-20.20s", navigation);

  for (uint8_t row = 0U; row < 3U; row++) {
    if (!force && strcmp(lines[row], previous[row]) == 0) {
      continue;
    }

    HAL_StatusTypeDef status = LcdSetCursor(row + 1U, 0);
    if (status != HAL_OK) {
      return status;
    }

    status = LcdPrint(lines[row]);
    if (status != HAL_OK) {
      return status;
    }

    memcpy(previous[row], lines[row], sizeof(previous[row]));
  }

  return HAL_OK;
}

static HAL_StatusTypeDef ConsoleShowTimerSettings(void) {
  HAL_StatusTypeDef status = LcdClear();
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(0, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("TIMER SETTINGS");
  if (status != HAL_OK) {
    return status;
  }

  return ConsoleUpdateTimerDisplay(1U);
}

static HAL_StatusTypeDef ConsoleShowLedControl(void) {
  const LedId leds[5] = {LED_GREEN, LED_YELLOW, LED_RED, LED_WHITE, LED_BLUE};

  char states[] = "G:0 Y:0 R:0 W:0 B:0";
  const uint8_t positions[5] = {2U, 6U, 10U, 14U, 18U};

  for (uint8_t i = 0; i < 5U; i++) {
    GPIO_PinState state;

    if (LedGetState(leds[i], &state) != HAL_OK) {
      return HAL_ERROR;
    }

    states[positions[i]] = (state == GPIO_PIN_SET) ? '1' : '0';
  }

  HAL_StatusTypeDef status = LcdClear();
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(0, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("LED CONTROL");
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(1, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint(states);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(2, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("1-5 Toggle");
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(3, 0);
  if (status != HAL_OK) {
    return status;
  }

  return LcdPrint("* Back   # All OFF");
}

static HAL_StatusTypeDef ConsoleUpdateSystemStatus(void) {
  char text[21];
  uint32_t seconds = HAL_GetTick() / 1000U;
  uint32_t clock_mhz = HAL_RCC_GetHCLKFreq() / 1000000U;

  snprintf(text, sizeof(text), "Up:%7lus %3luMHz ", (unsigned long)seconds,
           (unsigned long)clock_mhz);

  HAL_StatusTypeDef status = LcdSetCursor(1, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint(text);
  if (status != HAL_OK) {
    return status;
  }

  status = ConsoleUpdateTemperature();
  if (status == HAL_OK) {
    status_displayed_second = seconds;
  }

  return status;
}

static HAL_StatusTypeDef ConsoleShowSystemStatus(void) {
  HAL_StatusTypeDef status = LcdClear();
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(0, 0);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("SYSTEM STATUS");
  if (status != HAL_OK) {
    return status;
  }

  status = ConsoleUpdateSystemStatus();
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(3, 0);
  if (status != HAL_OK) {
    return status;
  }

  return LcdPrint("* = Back");
}

HAL_StatusTypeDef ConsoleInit(void) {
  /* The main loop initializes and services the LCD independently. */
  current_state = CONSOLE_MAIN_MENU;

  return ConsoleShowMainMenu();
}

HAL_StatusTypeDef ConsoleUpdate(char key) {
  if (key == '\0') {
    if (current_state == CONSOLE_SYSTEM_STATUS &&
        (HAL_GetTick() / 1000U) != status_displayed_second) {
      return ConsoleUpdateSystemStatus();
    }

    if (current_state == CONSOLE_TIMER_SETTINGS) {
      return ConsoleUpdateTimerDisplay(0U);
    }

    return HAL_OK;
  }

  switch (current_state) {
    case CONSOLE_MAIN_MENU:
      switch (key) {
        case '1':
          current_state = CONSOLE_LED_CONTROL;
          return ConsoleShowLedControl();

        case '2':
          current_state = CONSOLE_SYSTEM_STATUS;
          return ConsoleShowSystemStatus();

        case '3':
          current_state = CONSOLE_TIMER_SETTINGS;
          return ConsoleShowTimerSettings();

        default:
          break;
      }
      break;

    case CONSOLE_LED_CONTROL: {
      HAL_StatusTypeDef status = HAL_OK;
      LedId selected_led;

      switch (key) {
        case '1':
          selected_led = LED_GREEN;
          break;

        case '2':
          selected_led = LED_YELLOW;
          break;

        case '3':
          selected_led = LED_RED;
          break;

        case '4':
          selected_led = LED_WHITE;
          break;

        case '5':
          selected_led = LED_BLUE;
          break;

        case '#':
          for (LedId led = LED_GREEN; led < LED_COUNT; led++) {
            status = LedOff(led);
            if (status != HAL_OK) {
              return status;
            }
          }

          for (LedId led = LED_GREEN; led < LED_COUNT; led++) {
            status = ConsoleUpdateLedState(led);
            if (status != HAL_OK) {
              return status;
            }
          }

          return HAL_OK;

        case '*':
          current_state = CONSOLE_MAIN_MENU;
          return ConsoleShowMainMenu();

        default:
          return HAL_OK;
      }

      status = LedToggle(selected_led);
      if (status != HAL_OK) {
        return status;
      }

      return ConsoleUpdateLedState(selected_led);
    }

    case CONSOLE_SYSTEM_STATUS:
      if (key == '*') {
        current_state = CONSOLE_MAIN_MENU;
        return ConsoleShowMainMenu();
      }
      break;

    case CONSOLE_TIMER_SETTINGS:
      if (key == '*') {
        current_state = CONSOLE_MAIN_MENU;
        return ConsoleShowMainMenu();
      }

      if (key == '#') {
        CountdownState state = CountdownGetState();

        if (state == COUNTDOWN_RUNNING ||
            state == COUNTDOWN_COMPLETE) {
          CountdownReset();
          return HAL_OK;
        }

        if (timer_entry_seconds == 0U) {
          return HAL_OK;
        }

        if (!CountdownStart(timer_entry_seconds)) {
          return HAL_ERROR;
        }

        return HAL_OK;
      }

      /* Editing is allowed only while idle. */
      if (CountdownGetState() != COUNTDOWN_IDLE) {
        return HAL_OK;
      }

      if (key == 'D') {
        timer_entry_seconds = 0U;
        return ConsoleUpdateTimerDisplay(0U);
      }

      if (key >= '0' && key <= '9') {
        uint32_t candidate =
            timer_entry_seconds * 10U + (uint32_t)(key - '0');

        if (candidate <= 999U) {
          timer_entry_seconds = candidate;
          return ConsoleUpdateTimerDisplay(0U);
        }
      }
      break;

    default:
      current_state = CONSOLE_MAIN_MENU;
      return ConsoleShowMainMenu();
  }

  return HAL_OK;
}

static HAL_StatusTypeDef ConsoleUpdateLedState(LedId led) {
  static const uint8_t positions[LED_COUNT] = {[LED_GREEN] = 2U,
                                               [LED_YELLOW] = 6U,
                                               [LED_RED] = 10U,
                                               [LED_WHITE] = 14U,
                                               [LED_BLUE] = 18U};

  if ((unsigned int)led >= LED_COUNT) {
    return HAL_ERROR;
  }

  GPIO_PinState state;

  HAL_StatusTypeDef status = LedGetState(led, &state);
  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(1, positions[led]);
  if (status != HAL_OK) {
    return status;
  }

  char value[2] = {(state == GPIO_PIN_SET) ? '1' : '0', '\0'};

  return LcdPrint(value);
}

static HAL_StatusTypeDef ConsoleUpdateTemperature(void) {
  float temperature_c;
  char reading[32] = "MCU Temp: --.- C";
  char line[21];

  HAL_StatusTypeDef status = TemperatureReadCelsius(&temperature_c);

  if (status == HAL_OK &&
      temperature_c >= -40.0f && temperature_c <= 125.0f) {
    /* Round to tenths, then format using integer printing. */
    int32_t tenths = (int32_t)(temperature_c * 10.0f +
                             (temperature_c >= 0.0f ? 0.5f : -0.5f));

    uint32_t magnitude =
        (uint32_t)(tenths < 0 ? -tenths : tenths);

    snprintf(reading, sizeof(reading), "MCU Temp: %s%lu.%lu C",
             tenths < 0 ? "-" : "",
             (unsigned long)(magnitude / 10U),
             (unsigned long)(magnitude % 10U));
  }

  /* Pad the row to erase characters from the previous reading. */
  snprintf(line, sizeof(line), "%-20.20s", reading);

  status = LcdSetCursor(2, 0);
  if (status != HAL_OK) {
    return status;
  }

  return LcdPrint(line);
}
