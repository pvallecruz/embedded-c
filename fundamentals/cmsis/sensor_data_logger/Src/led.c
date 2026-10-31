/**
 * @file   led.c
 * @author Pedro Valle-Cruz
 * @brief  CMSIS register implementation for GPIOE LED control.
 */

#include "led.h"
#include "stm32f4xx.h"

/* Reject unsupported pins before shifts; PE10 belongs to the button. */
static int led_pin_is_valid(unsigned int pin) {
  return (pin == 7U) || (pin == 8U) || (pin == 9U) || (pin == 11U);
}

void led_init(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
  /* Set the latch low before enabling output mode. */
  GPIOE->BSRR = (1U << (pin + 16));
  /* Push-pull, no pulls, low speed; preserve other pins' settings. */
  GPIOE->OTYPER &= ~(1U << pin);
  GPIOE->PUPDR &= ~(3U << (pin * 2));
  GPIOE->OSPEEDR &= ~(3U << (pin * 2));
  GPIOE->MODER &= ~(3U << (pin * 2));
  GPIOE->MODER |= (1U << (pin * 2));
}

void led_on(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  GPIOE->BSRR = (1U << pin);
}

void led_off(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  GPIOE->BSRR = ((1U << pin) << 16U);
}

void led_toggle(unsigned int pin) {
  if (!led_pin_is_valid(pin)) {
    return;
  }
  /* Read the output latch. The read/modify sequence requires a single owner. */
  if ((GPIOE->ODR & (1U << pin)) == 0U) {
    led_on(pin);
  } else {
    led_off(pin);
  }
}
