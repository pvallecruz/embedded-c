/*
 * button.c
 *
 *  Created on: Oct 25, 2026
 *      Author: pedrol.valle-cruz
 */

#include "button.h"
#include "stm32f4xx.h"

static int button_raw;
static int button_stable;
static uint32_t button_changed_at;

void button_init(void) {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
	GPIOE->MODER &= ~(GPIO_MODER_MODER10);
	GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR10);
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR10_0;
	button_raw = button_is_pressed();
	button_stable = button_raw;
	button_changed_at = 0U;
}

int button_is_pressed(void) {
	return (GPIOE->IDR & GPIO_IDR_ID10) == 0U;
}

int button_update(uint32_t now) {
	int button_sample = button_is_pressed();
	if (button_sample != button_raw) {
		button_raw = button_sample;
		button_changed_at = now;
	}
	if ((button_raw != button_stable) && ((now - button_changed_at) >= 20U)) {
		button_stable = button_raw;
		if (button_stable == 1U) {
				return 1;
			}
	}

	return 0;
}



