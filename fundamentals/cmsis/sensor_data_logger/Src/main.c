/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : Button-controlled light sampling and timestamped USART2 output
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
#include "stm32f4xx.h"
#include "button.h"
#include "led.h"

#define ADC_TIMEOUT_MS 5U

static void adc_init(void);
static int adc_read(uint16_t *result);
static void usart2_init(void);
static void usart2_write_char(char character);
static void usart2_write_string(const char *text);
static void adc_recover(void);

/* Retain the latest result for debugger inspection, including while paused. */
volatile uint32_t adc_reading = 0U;
volatile uint32_t adc_timeout_count = 0U;
/* Written by SysTick; read by the polling loop and ADC startup wait. */
volatile uint32_t system_ticks = 0U;

int main(void) {
  /* Nominal reset HSI clock: 16 MHz, giving a 1 ms timebase. */
  SysTick_Config(16000U);
  adc_init();
  usart2_init();
  button_init();
  led_init(9U);
  uint32_t last_sample = system_ticks;
  uint8_t logging_enabled = 0U;
  char line[32];

  for (;;) {
    uint32_t now = system_ticks;
    /* Poll even while paused; consume each debounced press event once. */
    if (button_update(now)) {
      logging_enabled = !(logging_enabled);
      if (logging_enabled) {
        led_on(9U);
        /* Start a fresh interval rather than immediately sampling after a pause. */
        last_sample = now;
      } else {
        led_off(9U);
      }
    }

    /* Unsigned elapsed time tolerates tick wrap; no catch-up burst after delays. */
    if (logging_enabled && ((now - last_sample) >= 100U)) {
      uint16_t sample;
    	last_sample = now;
    	if (adc_read(&sample)) {
    		adc_reading = sample;
				/* Snapshot time precedes conversion; send raw ADC counts, not lux. */
				snprintf(line, sizeof(line), "%lu,%lu\r\n",
								 (unsigned long)now,
								 (unsigned long)adc_reading);
				usart2_write_string(line);
    	} else {
        adc_timeout_count++;
        logging_enabled = 0U;
        led_off(9U);
        adc_recover();
    	}
    }
  }
}

static void adc_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  /* Common ADC clock = APB2 / 2 = nominal 8 MHz. */
  ADC->CCR &= ~(ADC_CCR_ADCPRE);
  /* PA1: analog input without pulls, preserving other GPIOA pins. */
  GPIOA->MODER |= GPIO_MODER_MODER1;
  GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR1);
  /* One 12-bit channel-1 conversion, with a generous 480-cycle sample time. */
  ADC1->CR1 &= ~(ADC_CR1_RES);
  ADC1->SQR1 &= ~(ADC_SQR1_L);
  ADC1->SQR3 &= ~(ADC_SQR3_SQ1);
  ADC1->SQR3 |= ADC_SQR3_SQ1_0;
  ADC1->SMPR2 |= ADC_SMPR2_SMP1;
  ADC1->CR2 &= ~(ADC_CR2_CONT | ADC_CR2_EXTEN | ADC_CR2_ALIGN);
  ADC1->CR2 |= ADC_CR2_ADON;

  /* Two ticks ensure at least about 1 ms despite the first tick phase.
   * SysTick must already be running with interrupts enabled. */
  uint32_t started_at = system_ticks;
  while ((system_ticks - started_at) < 2U) {
  }
}

/* Blocking conversion; a hardware fault currently has no timeout recovery. */
static int adc_read(uint16_t *result) {
  uint32_t started_at = system_ticks;
	ADC1->CR2 |= (ADC_CR2_SWSTART);
  while ((ADC1->SR & ADC_SR_EOC) == 0U) {
  	if ((uint32_t)(system_ticks - started_at) >= ADC_TIMEOUT_MS) {
  		return 0;
  	}
  }
  /* Reading DR retrieves the result and clears EOC. */
  *result = (uint16_t)ADC1->DR;
  return 1;
}

/* Keep the interrupt short; application work remains in the main loop. */
void SysTick_Handler(void) {
  system_ticks++;
}

static void usart2_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  /* PA2 TX uses alternate-function mode and AF7; PA1 remains analog. */
  GPIOA->MODER &= ~(GPIO_MODER_MODER2);
  GPIOA->MODER |= GPIO_MODER_MODER2_1;
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2;
  GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL2_0 |
                    GPIO_AFRL_AFSEL2_1 |
                    GPIO_AFRL_AFSEL2_2);
  /* Reset-default 16x oversampling, 8N1; APB1 = 16 MHz, target 115200 baud. */
  USART2->BRR = (8U << USART_BRR_DIV_Mantissa_Pos) | 11U;
  USART2->CR1 |= (USART_CR1_UE | USART_CR1_TE);
}

/* TXE means DR can accept a byte, not that the last stop bit has left PA2. */
static void usart2_write_char(char character) {
  while ((USART2->SR & USART_SR_TXE) == 0U) {
  }
  USART2->DR = character;
}

static void usart2_write_string(const char *text) {
  while (*text != '\0') {
    usart2_write_char(*text);
    text++;
  }
}

static void adc_recover(void) {
	RCC->APB2RSTR |= RCC_APB2RSTR_ADCRST;
	RCC->APB2RSTR &= ~(RCC_APB2RSTR_ADCRST);
	adc_init();
}
