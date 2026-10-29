/**
 * @file   button.c
 * @author Pedro Valle-Cruz
 * @brief  HAL button input with nonblocking 20 ms debounce.
 */

#include "button.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* Persistent logical state: 1 = pressed, 0 = released. */
static int button_raw;
static int button_stable;
static uint32_t button_changed_at;

void button_init(void) {
  __HAL_RCC_GPIOE_CLK_ENABLE();
  GPIO_InitTypeDef config = {0};
  config.Pin = USER_BUTTON_Pin;
  config.Mode = GPIO_MODE_INPUT;
  config.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(USER_BUTTON_GPIO_Port, &config);
  /* Seed from hardware so a button held at startup is not a new event. */
  button_raw = button_is_pressed();
  button_stable = button_raw;
  /* No transition is pending; the first edge supplies its actual timestamp. */
  button_changed_at = 0U;
}

int button_is_pressed(void) {
  return HAL_GPIO_ReadPin(USER_BUTTON_GPIO_Port, USER_BUTTON_Pin) == GPIO_PIN_RESET;
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
