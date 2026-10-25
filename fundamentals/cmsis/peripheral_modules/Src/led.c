/*
 * led.c
 *
 *  Created on: Oct 25, 2026
 *      Author: pedrol.valle-cruz
 */

#include "led.h"
#include "stm32f4xx.h"

static int led_pin_is_valid(unsigned int pin) {
	return (pin == 7U) || (pin == 8U) || (pin == 9U) || (pin == 11U);
}

void led_init(unsigned int pin) {
	if (!led_pin_is_valid(pin)) {
		return;
	}
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
	GPIOE->BSRR = (1U << (pin + 16));
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
	if ((GPIOE->ODR & (1U << pin)) == 0U) {
		led_on(pin);
	} else {
		led_off(pin);
	}
}
