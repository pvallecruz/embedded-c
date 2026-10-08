/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Register-level button-controlled LED with SysTick debouncing
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

typedef struct {
	volatile uint32_t MODER;
	volatile uint32_t OTYPER;
	volatile uint32_t OSPEEDR;
	volatile uint32_t PUPDR;
	volatile uint32_t IDR;
	volatile uint32_t ODR;
	volatile uint32_t BSRR;
} gpio_registers_t;

typedef struct {
	volatile uint32_t CSR;
	volatile uint32_t RVR;
	volatile uint32_t CVR;
}systick_registers_t;

typedef struct {
	volatile uint32_t CR;
	volatile uint32_t PLLCFGR;
	volatile uint32_t CFGR;
	volatile uint32_t CIR;
	volatile uint32_t AHB1RSTR;
	volatile uint32_t AHB2RSTR;
	volatile uint32_t AHB3RSTR;
	uint32_t RESERVED0;
	volatile uint32_t APB1RSTR;
	volatile uint32_t APB2RSTR;
	uint32_t RESERVED1;
	uint32_t RESERVED2;
	volatile uint32_t AHB1ENR;
}rcc_registers_t;

gpio_registers_t * const pGPIOD = (gpio_registers_t *)0x40020C00U;
gpio_registers_t * const pGPIOA = (gpio_registers_t *)0x40020000U;
systick_registers_t * const pSYSTICK = (systick_registers_t *)0xE000E010U;
rcc_registers_t * const pRCC = (rcc_registers_t *)0x40023800U;

volatile uint32_t system_ticks = 0U;

void led_init(void);
void button_init(void);
void systick_init(void);
void led_on(void);
void led_off(void);
uint8_t button_is_pressed(void);

int main(void)
{
	led_init();
	button_init();

	uint8_t previous_button_state = 0U;
	uint8_t led_state = 0U;
	led_off();

	systick_init();
	uint8_t stable_button_state = 0U;
	uint32_t last_change_tick = system_ticks;

	while(1)
	{
		uint8_t current_button_state = button_is_pressed();

		if (current_button_state != previous_button_state)
		{
		    last_change_tick = system_ticks;
		}

		if((system_ticks - last_change_tick) >= 20U)
		{
			if(current_button_state != stable_button_state)
			{
				stable_button_state = current_button_state;
				if(stable_button_state == 1)
				{
					if(led_state == 0U)
					{
						led_on();
						led_state = 1U;
					}

					else
					{
						led_off();
						led_state = 0U;
					}

				}
			}
		}
		previous_button_state = current_button_state;
	}


}

void led_init(void)
{
	pRCC->AHB1ENR |= (1U << 3);
	pGPIOD->MODER &= ~(3U << 24);
	pGPIOD->MODER |= (1U << 24);

}

void button_init(void)
{
	pRCC->AHB1ENR |= (1U << 0);

	pGPIOA->MODER &= ~(3U << 0);

}

void systick_init(void)
{

	pSYSTICK->CSR &= ~(1U << 0);
	pSYSTICK->RVR = 15999U;
	pSYSTICK->CVR = 0U;

	pSYSTICK->CSR |= (1U << 2) | (1U << 1) | (1U << 0);

}

void led_on(void)
{

	pGPIOD->BSRR = (1U << 12);

}

void led_off(void)
{

	pGPIOD->BSRR = (1U << 28);
}

uint8_t button_is_pressed(void)
{

	uint32_t GPIOA_IDR_value = pGPIOA->IDR;

	uint8_t button_state = GPIOA_IDR_value & (1U << 0);

	return button_state;
}

void SysTick_Handler(void)
{
	system_ticks += 1;
}
