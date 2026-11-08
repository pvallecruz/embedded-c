/*
 * console.c
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */


#include "console.h"
#include "lcd.h"
#include "keypad.h"
#include "led.h"

static ConsoleState current_state = CONSOLE_MAIN_MENU;
static HAL_StatusTypeDef ConsoleUpdateLedState(LedId led);

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


static HAL_StatusTypeDef ConsoleShowSubmenu(const char *title) {
  HAL_StatusTypeDef status = LcdClear();

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(0, 0);

  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint(title);

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(2, 0);

  if (status != HAL_OK) {
    return status;
  }

  status = LcdPrint("Under construction");

  if (status != HAL_OK) {
    return status;
  }

  status = LcdSetCursor(3, 0);

  if (status != HAL_OK) {
    return status;
  }

  return LcdPrint("* = Back");
}


static HAL_StatusTypeDef ConsoleShowLedControl(void) {
  const LedId leds[5] = {
    LED_GREEN,
    LED_YELLOW,
    LED_RED,
    LED_WHITE,
    LED_BLUE
  };

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



HAL_StatusTypeDef ConsoleInit(void) {
  HAL_StatusTypeDef status = LcdInit();

  if (status != HAL_OK) {
    return status;
  }

  current_state = CONSOLE_MAIN_MENU;

  return ConsoleShowMainMenu();
}

HAL_StatusTypeDef ConsoleUpdate(void) {
  char key = KeypadGetKey();

  if (key == '\0') {
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
          return ConsoleShowSubmenu("SYSTEM STATUS");

        case '3':
          current_state = CONSOLE_TIMER_SETTINGS;
          return ConsoleShowSubmenu("TIMER SETTINGS");

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
      case CONSOLE_TIMER_SETTINGS:
        if (key == '*') {
          current_state = CONSOLE_MAIN_MENU;
          return ConsoleShowMainMenu();
        }
        break;

    default:
      current_state = CONSOLE_MAIN_MENU;
      return ConsoleShowMainMenu();
  }

  return HAL_OK;
}



static HAL_StatusTypeDef ConsoleUpdateLedState(LedId led) {
  static const uint8_t positions[LED_COUNT] = {
    [LED_GREEN] = 2U,
    [LED_YELLOW] = 6U,
    [LED_RED] = 10U,
    [LED_WHITE] = 14U,
    [LED_BLUE] = 18U
  };

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

  char value[2] = {
    (state == GPIO_PIN_SET) ? '1' : '0',
    '\0'
  };

  return LcdPrint(value);
}




