/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Nonblocking traffic light with debounced pedestrian requests
 *                   and a steady/blinking crossing indicator on GPIOE.
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

#include <stdint.h>
#include "stm32f4xx.h"

/* Vehicle phases; pedestrian indication is active only within requested red. */
typedef enum {
  TRAFFIC_RED,
  TRAFFIC_GREEN,
  TRAFFIC_YELLOW
} traffic_state_t;

/* PE10 is deliberately excluded: it is the active-low button input. */
#define RED_LED_PIN     7U
#define YELLOW_LED_PIN  8U
#define GREEN_LED_PIN   9U
#define PEDESTRIAN_LED_PIN 11U

void traffic_light_init(void);
void button_init(void);
static void traffic_light_on(unsigned int pin);
static void traffic_light_off(unsigned int pin);
static void pedestrian_light_toggle(void);
void SysTick_Handler(void);

/* SysTick writes this counter; main reads snapshots. Nominal units are milliseconds. */
static volatile uint32_t system_ticks = 0U;

int main(void) {
  traffic_light_init();
  button_init();

  /* Seed debounce from the pin: a button held at startup is not a new press. */
  uint32_t button_raw = (GPIOE->IDR >> 10) & 1U;
  uint32_t button_stable = button_raw;
  uint32_t button_changed_at = system_ticks;
  uint32_t pedestrian_last_toggle = system_ticks;

  /* Assumes reset HSI at 16 MHz. CMSIS sets LOAD to 15999 for a 1 ms tick. */
  if (SysTick_Config(16000U) != 0U) {
    for (;;) {
      /* Stop if the requested tick interval is invalid. */
    }
  }
  traffic_state_t state = TRAFFIC_RED;
  uint32_t state_started = system_ticks;
  traffic_light_on(RED_LED_PIN);
  unsigned int pedestrian_requested = 0U;

  for (;;) {
    /* One time snapshot per iteration; unsigned subtraction tolerates wraparound. */
    uint32_t now = system_ticks;
    uint32_t button_sample = (GPIOE->IDR >> 10) & 1U;
    /* Every raw edge restarts the 20 ms stability interval without blocking. */
    if (button_sample != button_raw) {
      button_raw = button_sample;
      button_changed_at = now;
    }

    if ((button_raw != button_stable) &&
        ((now - button_changed_at) >= 20U)) {
      button_stable = button_raw;
      /* Accept only a debounced press during green; holding does not repeat it. */
      if ((button_stable == 0U) && (state == TRAFFIC_GREEN)) {
        pedestrian_requested = 1U;
      }
    }

    switch (state) {
    case TRAFFIC_RED:
      /* End red before considering blinking, keeping white off before green. */
      if ((now - state_started) >= 10000U) {
        traffic_light_off(RED_LED_PIN);
        traffic_light_off(PEDESTRIAN_LED_PIN);
        /* Consume the request only after its full crossing phase. */
        pedestrian_requested = 0U;
        traffic_light_on(GREEN_LED_PIN);
        state = TRAFFIC_GREEN;
        state_started = now;
      } else if ((pedestrian_requested == 1U) &&
                 ((now - state_started) >= 8000U)) {
        /* First toggle occurs at 8 s of red, then every 250 ms until red ends. */
        if ((now - pedestrian_last_toggle) >= 250U) {
          pedestrian_light_toggle();
          pedestrian_last_toggle = now;
        }
      }
      break;

    case TRAFFIC_GREEN:
      /* A request can shorten green, but never below 2 s; otherwise use 10 s. */
      if (((now - state_started) >= 10000U) ||
          ((pedestrian_requested == 1U) &&
           ((now - state_started) >= 2000U))) {
        traffic_light_off(GREEN_LED_PIN);
        traffic_light_on(YELLOW_LED_PIN);
        state = TRAFFIC_YELLOW;
        state_started = now;
      }
      break;

    case TRAFFIC_YELLOW:
      if ((now - state_started) >= 5000U) {
        traffic_light_off(YELLOW_LED_PIN);
        traffic_light_on(RED_LED_PIN);
        if (pedestrian_requested == 1U) {
          /* Red is already on; preserve the request throughout the crossing. */
          traffic_light_on(PEDESTRIAN_LED_PIN);
        }
        state = TRAFFIC_RED;
        state_started = now;
        pedestrian_last_toggle = now;
      }
      break;
    }
  }
}

/* Configure four LED outputs, leaving PE10 for button_init().
 * Other GPIO properties and initial low outputs rely on hardware reset defaults. */
void traffic_light_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
  GPIOE->MODER &= ~(GPIO_MODER_MODER7 | GPIO_MODER_MODER8 |
                    GPIO_MODER_MODER9 | GPIO_MODER_MODER11);
  GPIOE->MODER |= (GPIO_MODER_MODER7_0 | GPIO_MODER_MODER8_0 |
                   GPIO_MODER_MODER9_0 | GPIO_MODER_MODER11_0);
}

/* GPIOE clock must already be enabled. Released = 1; pressed connects PE10 to GND. */
void button_init(void) {
  GPIOE->MODER &= ~(GPIO_MODER_MODER10);
  GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR10);
  GPIOE->PUPDR |= GPIO_PUPDR_PUPDR10_0;
}

/* Accept only LED pins. Direct BSRR writes leave unrelated outputs unchanged. */
static void traffic_light_on(unsigned int pin) {
  if (((pin >= RED_LED_PIN) && (pin <= GREEN_LED_PIN)) ||
      (pin == PEDESTRIAN_LED_PIN)) {
    GPIOE->BSRR = (1U << pin);
  }
}

/* BSRR bits 16-31 reset the corresponding output pins. */
static void traffic_light_off(unsigned int pin) {
  if (((pin >= RED_LED_PIN) && (pin <= GREEN_LED_PIN)) ||
      (pin == PEDESTRIAN_LED_PIN)) {
    GPIOE->BSRR = (1U << (pin + 16U));
  }
}

/* Read the output latch; main is the sole writer of these LED outputs. */
static void pedestrian_light_toggle(void) {
  if ((GPIOE->ODR & (1U << PEDESTRIAN_LED_PIN)) != 0U) {
    traffic_light_off(PEDESTRIAN_LED_PIN);
  } else {
    traffic_light_on(PEDESTRIAN_LED_PIN);
  }
}

/* Keep the ISR short; all button and light decisions stay in main. */
void SysTick_Handler(void) {
  system_ticks++;
}
