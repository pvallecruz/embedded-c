
/**
 * @file led.c
 * @brief HAL driver for five active-high LEDs on GPIOE.
 */

#include "led.h"

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
} LedConfig;

static const LedConfig led_config[LED_COUNT] = {
  [LED_GREEN]  = {GPIOE, GPIO_PIN_7},
  [LED_YELLOW] = {GPIOE, GPIO_PIN_8},
  [LED_RED]    = {GPIOE, GPIO_PIN_9},
  [LED_WHITE]  = {GPIOE, GPIO_PIN_10},
  [LED_BLUE]   = {GPIOE, GPIO_PIN_11}
};

static uint8_t LedIsValid(LedId led) {
  return (unsigned int)led < (unsigned int)LED_COUNT;
}

void LedInit(void) {
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitTypeDef config = {0};

  config.Pin = GPIO_PIN_7 |
               GPIO_PIN_8 |
               GPIO_PIN_9 |
               GPIO_PIN_10 |
               GPIO_PIN_11;

  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_WritePin(GPIOE, config.Pin, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOE, &config);
}

HAL_StatusTypeDef LedOn(LedId led) {
  if (!LedIsValid(led)) {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(
      led_config[led].port,
      led_config[led].pin,
      GPIO_PIN_SET
  );

  return HAL_OK;
}

HAL_StatusTypeDef LedOff(LedId led) {
  if (!LedIsValid(led)) {
    return HAL_ERROR;
  }

  HAL_GPIO_WritePin(
      led_config[led].port,
      led_config[led].pin,
      GPIO_PIN_RESET
  );

  return HAL_OK;
}

HAL_StatusTypeDef LedToggle(LedId led) {
  if (!LedIsValid(led)) {
    return HAL_ERROR;
  }

  HAL_GPIO_TogglePin(
      led_config[led].port,
      led_config[led].pin
  );

  return HAL_OK;
}


HAL_StatusTypeDef LedGetState(LedId led, GPIO_PinState *state) {
  if (!LedIsValid(led) || state == NULL) {
    return HAL_ERROR;
  }

  GPIO_TypeDef *port = led_config[led].port;
  uint16_t pin = led_config[led].pin;

  *state = ((port->ODR & pin) != 0U)
               ? GPIO_PIN_SET
               : GPIO_PIN_RESET;

  return HAL_OK;
}

