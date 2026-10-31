/**
 * @file   button.c
 * @author Pedro Valle-Cruz
 * @brief  CMSIS button input with nonblocking 20 ms debounce.
 */

#include "button.h"
#include "stm32f4xx.h"

/* Private persistent state uses logical pressed values, not pin voltage. */
static int button_raw;
static int button_stable;
static uint32_t button_changed_at;

void button_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
  GPIOE->MODER &= ~(GPIO_MODER_MODER10);
  GPIOE->PUPDR &= ~(GPIO_PUPDR_PUPDR10);
  GPIOE->PUPDR |= GPIO_PUPDR_PUPDR10_0;
  /* Holding at startup is an initial state, not a new press. */
  button_raw = button_is_pressed();
  button_stable = button_raw;
  /* No transition is pending; the first raw change supplies its timestamp. */
  button_changed_at = 0U;
}

int button_is_pressed(void) {
  return (GPIOE->IDR & GPIO_IDR_ID10) == 0U;
}

int button_update(uint32_t now) {
  int button_sample = button_is_pressed();

  /* Every raw edge restarts the stability interval. */
  if (button_sample != button_raw) {
    button_raw = button_sample;
    button_changed_at = now;
  }

  /* Unsigned subtraction handles wrap when polled regularly. */
  if ((button_raw != button_stable) &&
      ((now - button_changed_at) >= 20U)) {
    button_stable = button_raw;
    /* Report the press transition once; release only updates the state. */
    if (button_stable == 1U) {
      return 1;
    }
  }

  return 0;
}
