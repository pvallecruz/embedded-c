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

uint32_t RCC_BaseAddress = 0x40023800U;
uint32_t RCC_OffSet = 0x30U;
uint32_t GPIOx_BaseAddress = 0x40020C00U;
uint32_t GPIOx_IDR_OffSet = 0x10U;
uint32_t GPIOx_BSRR_OffSet = 0x18U;
uint32_t GPIOA_BaseAddress = 0x40020000U;
uint32_t SYST_CSR_Address = 0xE000E010U;
uint32_t SYST_RVR_Address = 0xE000E014U;
uint32_t SYST_CVR_Address = 0xE000E018U;

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
	volatile uint32_t *pRCC_AHB1ENR_GPIODEN = (uint32_t*)(RCC_BaseAddress + RCC_OffSet);
	*pRCC_AHB1ENR_GPIODEN |= (1U << 3);

	volatile uint32_t *pGPIOD_Moder12 = (uint32_t*)(GPIOx_BaseAddress);
	*pGPIOD_Moder12 &= ~(3U << 24);
	*pGPIOD_Moder12 |= (1U << 24);

}

void button_init(void)
{
	volatile uint32_t *pRCC_AHB1ENR_GPIOAEN = (uint32_t*)(RCC_BaseAddress + RCC_OffSet);
	*pRCC_AHB1ENR_GPIOAEN |= (1U << 0);

	volatile uint32_t *pGPIOA_Moder0 = (uint32_t*)(GPIOA_BaseAddress);
	*pGPIOA_Moder0 &= ~(3U << 0);

}

void systick_init(void)
{
	volatile uint32_t *pSTK_CTRL_Enable = (uint32_t*)(SYST_CSR_Address);
	*pSTK_CTRL_Enable &= ~(1U << 0);
	volatile uint32_t *pSTK_LOAD = (uint32_t*)(SYST_RVR_Address);
	*pSTK_LOAD = 15999U;
	volatile uint32_t *pSTK_VAL = (uint32_t*)(SYST_CVR_Address);
	*pSTK_VAL = 0U;

	*pSTK_CTRL_Enable |= (1U << 2) | (1U << 1) | (1U << 0);

}

void led_on(void)
{

	volatile uint32_t *pGPIOD_BSRR = (uint32_t*)(GPIOx_BaseAddress + GPIOx_BSRR_OffSet);
	*pGPIOD_BSRR = (1U << 12);

}

void led_off(void)
{
	volatile uint32_t *pGPIOD_BSRR = (uint32_t*)(GPIOx_BaseAddress + GPIOx_BSRR_OffSet);
	*pGPIOD_BSRR = (1U << 28);
}

uint8_t button_is_pressed(void)
{
	volatile uint32_t *pGPIOA_IDR = (uint32_t*)(GPIOA_BaseAddress +GPIOx_IDR_OffSet);
	uint32_t GPIOA_IDR_value = *pGPIOA_IDR;

	uint8_t button_state = GPIOA_IDR_value & (1U << 0);

	return button_state;
}

void SysTick_Handler(void)
{
	system_ticks += 1;
}
