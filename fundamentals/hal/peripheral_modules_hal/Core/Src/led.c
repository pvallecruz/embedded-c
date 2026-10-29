/**
 * @file   led.c
 * @author Pedro Valle-Cruz
 * @brief  HAL implementation of validated GPIOE LED control.
 */

#include "led.h"
#include "stm32f4xx_hal.h"

/* Validate before shifts; PE10 is reserved for the button. */
static int led_pin_is_valid(unsigned int pin) {
  return (pin == 7U) || (pin == 8U) || (pin == 9U) || (pin == 11U);
}

void led_init(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef config = { 0 };
  /* The public API accepts a pin number; HAL expects a bit mask. */
  config.Pin = (1U << pin);
  config.Mode = GPIO_MODE_OUTPUT_PP;
  config.Pull = GPIO_NOPULL;
  config.Speed = GPIO_SPEED_FREQ_LOW;
  /* Clear the output latch before applying output mode. */
  HAL_GPIO_WritePin(GPIOE, config.Pin, GPIO_PIN_RESET);
  HAL_GPIO_Init(GPIOE, &config);
}

void led_on(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  HAL_GPIO_WritePin(GPIOE, 1U << pin, GPIO_PIN_SET);
}

void led_off(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  HAL_GPIO_WritePin(GPIOE, 1U << pin, GPIO_PIN_RESET);
}

void led_toggle(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  /* HAL reads the output latch: callers must maintain exclusive ownership. */
  HAL_GPIO_TogglePin(GPIOE, 1U << pin);
}
