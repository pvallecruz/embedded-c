/**
 * @file   led.h
 * @author Pedro Valle-Cruz
 * @brief  Initialize and control active-high LEDs on GPIOE.
 */

#ifndef INC_LED_H_
#define INC_LED_H_

#include "stm32f4xx_hal.h"

typedef enum {
  LED_GREEN = 0,
  LED_YELLOW,
  LED_RED,
  LED_WHITE,
  LED_BLUE,
  LED_COUNT
} LedId;

void LedInit(void);

HAL_StatusTypeDef LedOn(LedId led);
HAL_StatusTypeDef LedOff(LedId led);
HAL_StatusTypeDef LedToggle(LedId led);
HAL_StatusTypeDef LedGetState(LedId led, GPIO_PinState *state);

#endif /* INC_LED_H_ */
