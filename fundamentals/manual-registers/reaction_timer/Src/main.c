/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Register-level reaction game with varying delays and best-time tracking
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
#include <stdio.h>
#include <stdlib.h>

// Partial GPIO layout through BSRR; member order preserves register offsets.
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
} systick_registers_t;

// Reserved words preserve the gaps so AHB1ENR lands at offset 0x30.
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
} rcc_registers_t;

typedef enum {
    GAME_READY,
    GAME_WAITING,
    GAME_MEASURING,
    GAME_RESULT,
	GAME_WAIT_RELEASE,
	GAME_TOO_SOON
} game_state_t;

#define DEBOUNCE_MS 20U

// These pointers map hardware, not RAM objects. Their addresses stay fixed;
// volatile members ensure register accesses occur when requested.
gpio_registers_t * const pGPIOD = (gpio_registers_t *)0x40020C00U;
gpio_registers_t * const pGPIOA = (gpio_registers_t *)0x40020000U;
systick_registers_t * const pSYSTICK = (systick_registers_t *)0xE000E010U;
rcc_registers_t * const pRCC = (rcc_registers_t *)0x40023800U;

// Shared with SysTick_Handler; one tick is nominally 1 ms at 16 MHz.
volatile uint32_t system_ticks = 0U;

void led_init(void);
void button_init(void);
void systick_init(void);
void led_on(void);
void led_off(void);
uint8_t button_is_pressed(void);
void SysTick_Handler(void);

int main(void) {
	led_init();
	button_init();
	led_off();
	systick_init();

	game_state_t game_state = GAME_READY;

	uint32_t wait_start_tick = 0U;
	uint32_t reaction_start_tick = 0U;
	uint32_t reaction_time_ms = 0U;
	uint32_t wait_duration_ms = 2000U;
	uint8_t random_seeded = 0U;
	// The first valid result establishes the record; reset clears this RAM value.
	uint32_t best_time_ms = UINT32_MAX;
	uint8_t ready_message_shown = 0U;
	uint8_t previous_button_state = 0U;
	uint8_t stable_button_state = 0U;
	uint32_t last_change_tick = system_ticks;
	uint32_t accepted_press_tick = 0U;

	for (;;) {
		uint8_t current_button_state = button_is_pressed();

		// Every raw edge restarts the settling interval, including contact bounce.
		if (current_button_state != previous_button_state) {
			last_change_tick = system_ticks;
			previous_button_state = current_button_state;
		}

		// Accept only a stable input change; unsigned elapsed time tolerates wrap.
		if ((system_ticks - last_change_tick) >= DEBOUNCE_MS) {
			if(current_button_state != stable_button_state) {
				stable_button_state = current_button_state;
				if(stable_button_state == 1U) {
					// Use the start of the stable press, not its later debounce confirmation.
					accepted_press_tick = last_change_tick;
				}
			}
		}

		switch (game_state) {
			case GAME_READY:
				// Print once per entry to READY rather than on every loop iteration.
				if (ready_message_shown == 0U) {
					printf("Press the button to start the game\n");
					ready_message_shown = 1U;

				}

				if (stable_button_state == 1U) {
					game_state = GAME_WAIT_RELEASE;
				}

				break;

			// Require release so the start press cannot become the reaction press.
			case GAME_WAIT_RELEASE:
				if (stable_button_state == 0U) {
					// Seed once from user timing; this varies rounds but is not true randomness.
					if (random_seeded == 0U) {
						srand(system_ticks);
						random_seeded = 1U;
					}
					// Choose an inclusive 2000-5000 ms delay once per round.
					wait_duration_ms = 2000U + ((uint32_t)rand() % 3001U);
					wait_start_tick = system_ticks;
					game_state = GAME_WAITING;
				}

				break;

			case GAME_WAITING:
				if (stable_button_state == 1U) {
					game_state = GAME_TOO_SOON;

				} else if ((system_ticks - wait_start_tick) >=
						    wait_duration_ms) {
					led_on();
					reaction_start_tick = system_ticks;
					game_state = GAME_MEASURING;
				}

				break;

			case GAME_MEASURING:
				if (stable_button_state == 1U) {
					// A press may begin before the LED but finish debouncing afterward.
					// Compare offsets from the same round start to detect that early press.
					if ((accepted_press_tick - wait_start_tick) <
					    (reaction_start_tick - wait_start_tick)) {
						led_off();
						game_state = GAME_TOO_SOON;
					} else {
						reaction_time_ms = accepted_press_tick - reaction_start_tick;

						if (reaction_time_ms < best_time_ms) {
							best_time_ms = reaction_time_ms;
							printf("New best time!\n");
						}

						printf("Reaction time: %lu ms\n", (unsigned long)reaction_time_ms);
						printf("Best time: %lu ms\n\n", (unsigned long)best_time_ms);

						led_off();
						game_state = GAME_RESULT;
					}
				}

				break;

			// Wait for release before allowing another round and showing its prompt.
			case GAME_RESULT:
				if (stable_button_state == 0U) {
					ready_message_shown = 0U;
					game_state = GAME_READY;
				}

				break;

			// Delay feedback and rearming until the rejected press is released.
			case GAME_TOO_SOON:
				if (stable_button_state == 0U) {
					printf("Too soon!\n\n");
					ready_message_shown = 0U;
					game_state = GAME_READY;
				}

				break;

			default:

				break;
		}
	}
}

void led_init(void) {
	pRCC->AHB1ENR |= (1U << 3);
	pGPIOD->MODER &= ~(3U << 24);
	pGPIOD->MODER |= (1U << 24);

}

void button_init(void) {
	pRCC->AHB1ENR |= (1U << 0);
	pGPIOA->MODER &= ~(3U << 0);

}

void systick_init(void) {
	pSYSTICK->CSR &= ~(1U << 0);
	// At 16 MHz, 16000 processor cycles give a nominal 1 ms interrupt period.
	pSYSTICK->RVR = 15999U;
	// Any write clears the current count before the timer is restarted.
	pSYSTICK->CVR = 0U;
	pSYSTICK->CSR |= (1U << 2) | (1U << 1) | (1U << 0);

}

void led_on(void) {

	// BSRR sets PD12 without a read-modify-write of other output pins.
	pGPIOD->BSRR = (1U << 12);

}

void led_off(void) {

	// BSRR upper-half bit 16 + 12 resets PD12; write 1 to issue the command.
	pGPIOD->BSRR = (1U << 28);
}

uint8_t button_is_pressed(void) {

	// The onboard USER button drives PA0 high when pressed.
	uint8_t button_state = (pGPIOA->IDR) & (1U << 0);
	return button_state;
}

void SysTick_Handler(void) {
	system_ticks++;
}

