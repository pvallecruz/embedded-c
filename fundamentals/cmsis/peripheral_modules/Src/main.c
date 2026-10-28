/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Toggle an LED once per debounced button press using CMSIS modules.
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
#include "led.h"
#include "button.h"
#include "stm32f4xx.h"

void SysTick_Handler(void);

/* Written by SysTick; the foreground loop reads millisecond snapshots. */
static volatile uint32_t system_ticks = 0U;

int main(void) {
  button_init();
  led_init(7U);

  /* Reset clock assumption: nominal 16 MHz HSI gives a 1 ms interrupt. */
  if (SysTick_Config(16000U) != 0U) {
    for (;;) {
      /* Stop if the requested SysTick interval cannot be configured. */
    }
  }

  while (1) {
    uint32_t now = system_ticks;
    /* Consume each event once; holding and release leave the LED unchanged. */
    if (button_update(now) == 1U) {
      led_toggle(7U);
    }
  }
}

/* Keep interrupt work short; debounce and GPIO decisions run in main. */
void SysTick_Handler(void) {
  system_ticks++;
}
