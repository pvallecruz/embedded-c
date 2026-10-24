/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Pedro Valle-Cruz
 * @brief          : USART2 interrupt-driven command console with buffered reception,
 *                   input validation, error recovery, and four-LED control.
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
#include <string.h>
#include <stdlib.h>
#include "stm32f4xx.h"

/* Leave one slot unused: a 64-slot ring holds 63 unread characters. */
#define RX_BUFFER_SIZE 64U

/* ISR produces at head; main consumes at tail and flushes under interrupt masking.
 * Volatile exposes shared accesses; the critical section provides coordination. */
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile unsigned int rx_head = 0U;
static volatile unsigned int rx_tail = 0U;
/* ISR-owned counters: main compares snapshots rather than clearing them. */
static volatile uint32_t rx_dropped_count = 0U;
static volatile uint32_t rx_overrun_count = 0U;
static volatile uint32_t rx_framing_count = 0U;
static volatile uint32_t rx_noise_count = 0U;

static void usart2_init(void);
static void usart2_write_char(char character);
static void usart2_write_string(const char *text);
static void process_command(char *command);
static void led_init(void);
static void led_pin_on(unsigned int pin);
static void led_pin_off(unsigned int pin);
static uint8_t led_pin_status(unsigned int pin);
void USART2_IRQHandler(void);
static uint8_t usart2_try_read_char(char *character);

int main(void) {
  usart2_init();
  led_init();
  usart2_write_string("Hello, Pedro!\r\n");
  /* Separate from the receive queue: reserve the final slot for a null terminator. */
  char line[64];
  unsigned int length = 0U;
  unsigned int line_overflow = 0U;
  uint32_t last_dropped_count = 0U;
  uint32_t last_overrun_count = 0U;
  uint32_t last_framing_count = 0U;
  uint32_t last_noise_count = 0U;
  unsigned int discard_line = 0U;

  for (;;) {
    /* Keep recovery and dequeue together; never print or wait while masked. */
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();

    if ((rx_dropped_count != last_dropped_count) ||
        (rx_overrun_count != last_overrun_count) ||
        (rx_framing_count != last_framing_count) ||
        (rx_noise_count != last_noise_count)) {
        last_dropped_count = rx_dropped_count;
        last_overrun_count = rx_overrun_count;
        last_framing_count = rx_framing_count;
        last_noise_count = rx_noise_count;

        /* Flush queued delimiters too; only a later line ending may finish recovery. */
        rx_tail = rx_head;
        length = 0U;
        line_overflow = 0U;
        discard_line = 1U;
    }
    char received;
    uint8_t has_character = usart2_try_read_char(&received);

    /* Restore the caller's mask state instead of unconditionally enabling IRQs. */
    __set_PRIMASK(saved_primask);

    if (has_character == 0U) {
        continue;
    }
    /* Reject the rest of a damaged line before accepting another command. */
    if (discard_line != 0U) {
        if (received == '\r' || received == '\n') {
            discard_line = 0U;
            usart2_write_string("\r\nError: receive data lost; retype command\r\n");
        }
        continue;
    }
    if (received == '\r' || received == '\n') {
      /* Tokenization and string comparisons require a terminated line. */
      line[length] = '\0';
      if (line_overflow != 0U) {
          usart2_write_string("\r\nError: line too long\r\n");
      } else if (length > 0U) {
        process_command(line);
      }
      length = 0U;
      line_overflow = 0U;
    } else if (received == '\b' || received == 127) {
        if (length > 0U) {
            length--;
            /* Move back, erase the displayed character, then move back again. */
            usart2_write_string("\b \b");
        }
    } else if (length < 63U) {
      line[length] = received;
      length++;
      usart2_write_char(received);
    } else {
        /* Once too long, reject the whole line even if Backspace later frees space. */
        line_overflow = 1U;
    }
  }
}

/* Assumes reset clock settings: nominal 16 MHz HSI, APB1 undivided, 8N1 UART. */
static void usart2_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  /* PA2 TX and PA3 RX use alternate-function mode and AF7. */
  GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
  GPIOA->MODER |= (GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1);
  GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
  GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL2_0 | GPIO_AFRL_AFSEL2_1 | GPIO_AFRL_AFSEL2_2);
  GPIOA->AFR[0] |= (GPIO_AFRL_AFSEL3_0 | GPIO_AFRL_AFSEL3_1 | GPIO_AFRL_AFSEL3_2);
  /* 16x oversampling, 115200 target baud: rounded divider is 8 + 11/16. */
  USART2->BRR = (8U << USART_BRR_DIV_Mantissa_Pos) | 11U;
  USART2->CR1 |= (USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);
  /* Enable receive requests at the peripheral and service them through the NVIC. */
  USART2->CR1 |= USART_CR1_RXNEIE;
  NVIC_EnableIRQ(USART2_IRQn);
}

/* Transmission remains blocking. TXE means DR is free, not that TX is complete. */
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

/* strtok modifies this writable, null-terminated line; parsing stays outside the ISR. */
static void process_command(char *command) {
  if (strcmp(command, "help") == 0) {
    usart2_write_string("\r\nCommands: help, led <12-15> <on|off|status|toggle>\r\n");
  } else {
    char *name = strtok(command, " ");
    char *pin_text = strtok(NULL, " ");
    char *action = strtok(NULL, " ");
    /* A fourth token rejects trailing input instead of executing a valid prefix. */
    char *extra = strtok(NULL, " ");
    if (name == NULL || pin_text == NULL ||
      action == NULL || extra != NULL) {
      usart2_write_string("\r\nUsage: led <12-15> <on|off|status|toggle>\r\n");
      return;
    }
    if (strcmp(name, "led") != 0) {
      usart2_write_string("\r\nUnknown command\r\n");
      return;
    }
    /* Require digits, no trailing text, and a valid pin before casting or shifting. */
    char *end;
    long pin = strtol(pin_text, &end, 10);
    if (end == pin_text || *end != '\0' || pin < 12 || pin > 15) {
      usart2_write_string("\r\nError: pin must be 12-15\r\n");
      return;
    }
    if ((strcmp(action, "on") == 0U)) {
      led_pin_on((unsigned int)pin);
      usart2_write_string("\r\nLED ");
      usart2_write_string(pin_text);
      usart2_write_string(" on\r\n");
    } else if ((strcmp(action, "off") == 0U)) {
      led_pin_off((unsigned int)pin);
      usart2_write_string("\r\nLED ");
      usart2_write_string(pin_text);
      usart2_write_string(" off\r\n");
    } else if ((strcmp(action, "status") == 0U)) {
      usart2_write_string("\r\nLED ");
      usart2_write_string(pin_text);
      if (led_pin_status((unsigned int)pin) == 1U) {
        usart2_write_string(" on\r\n");
      } else {
        usart2_write_string(" off\r\n");
      }
    } else if ((strcmp(action, "toggle") == 0U)) {
      usart2_write_string("\r\nLED ");
      usart2_write_string(pin_text);
      if (led_pin_status((unsigned int)pin) == 1U) {
        led_pin_off((unsigned int)pin);
        usart2_write_string(" off\r\n");
      } else {
        led_pin_on((unsigned int)pin);
        usart2_write_string(" on\r\n");
      }
    } else {
      usart2_write_string("\r\nUnknown command\r\n");
    }
  }
}

/* Active-high onboard LEDs: PD12 green, PD13 orange, PD14 red, PD15 blue. */
static void led_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
  GPIOD->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13 | GPIO_MODER_MODER14 |
                    GPIO_MODER_MODER15);
  GPIOD->MODER |= (GPIO_MODER_MODER12_0 | GPIO_MODER_MODER13_0 | GPIO_MODER_MODER14_0 |
                   GPIO_MODER_MODER15_0);
}

/* Read the output latch, not optical feedback; invalid pins also return zero. */
static uint8_t led_pin_status(unsigned int pin) {
  if ((pin >= 12) && (pin <=15)) {
    if ((GPIOD->ODR & (1U << pin)) != 0U) {
      return 1U;
    }
  }
  return 0U;
}

static void led_pin_on(unsigned int pin) {
  if ((pin >= 12) && (pin <=15)) {
    /* Direct BSRR writes affect only the selected pin. Lower half sets outputs. */
    GPIOD->BSRR = (1U << pin);
  }
}

static void led_pin_off(unsigned int pin) {
  if ((pin >= 12) && (pin <=15)) {
    /* Upper half resets outputs; no read-modify-write is needed. */
    GPIOD->BSRR = (1U << (pin + 16));
  }
}

/* Keep the vector-visible handler short: acknowledge, count errors, or enqueue. */
void USART2_IRQHandler(void) {
  /* Save flags before DR: the SR-then-DR sequence clears receive error flags. */
  uint32_t status = USART2->SR;
  if (status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
    char received = (char)USART2->DR;
    /* Separate checks count simultaneous errors before discarding the byte. */
    if (status & USART_SR_ORE) {
        rx_overrun_count++;
    }
    if (status & USART_SR_FE) {
        rx_framing_count++;
    }
    /* The manual calls this flag NF; the supplied CMSIS macro is USART_SR_NE. */
    if (status & USART_SR_NE) {
        rx_noise_count++;
    }

    if (status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
        return;
    }
    unsigned int next_head = (rx_head + 1U) % RX_BUFFER_SIZE;
    if (next_head != rx_tail) {
        rx_buffer[rx_head] = received;
        /* Publish the new head only after the character has been stored. */
        rx_head = next_head;
    } else {
        /* Preserve unread bytes; reject the newest byte and notify main by count. */
        rx_dropped_count++;
    }
  }
}

/* Called with interrupts masked. Return immediately; release a slot after copying. */
static uint8_t usart2_try_read_char(char *character) {
  if (rx_head == rx_tail) {
    return 0U;
  }
    *character = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1U) % RX_BUFFER_SIZE;
    return 1U;
}
