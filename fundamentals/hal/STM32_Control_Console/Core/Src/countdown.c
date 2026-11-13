/*
 * countdown.c
 *
 *  Created on: Nov 13, 2026
 *      Author: pedrol.valle-cruz
 */

#include "countdown.h"
#include "stm32f4xx_hal.h"

#define COUNTDOWN_MAX_SECONDS 999U

static CountdownState countdown_state = COUNTDOWN_IDLE;
static uint32_t start_time = 0U;
static uint32_t duration_ms = 0U;

void CountdownReset(void) {
  countdown_state = COUNTDOWN_IDLE;
  start_time = 0U;
  duration_ms = 0U;
}

uint8_t CountdownStart(uint32_t seconds) {
  if (seconds == 0U || seconds > COUNTDOWN_MAX_SECONDS) {
    return 0U;
  }

  duration_ms = seconds * 1000U;
  start_time = HAL_GetTick();
  countdown_state = COUNTDOWN_RUNNING;

  return 1U;
}

uint8_t CountdownUpdate(void) {
  if (countdown_state != COUNTDOWN_RUNNING) {
    return 0U;
  }

  uint32_t elapsed = (uint32_t)(HAL_GetTick() - start_time);

  if (elapsed < duration_ms) {
    return 0U;
  }

  countdown_state = COUNTDOWN_COMPLETE;
  return 1U;
}

CountdownState CountdownGetState(void) {
  return countdown_state;
}

uint32_t CountdownGetRemainingSeconds(void) {
  if (countdown_state != COUNTDOWN_RUNNING) {
    return 0U;
  }

  uint32_t elapsed = (uint32_t)(HAL_GetTick() - start_time);

  if (elapsed >= duration_ms) {
    return 0U;
  }

  uint32_t remaining_ms = duration_ms - elapsed;

  return (remaining_ms + 999U) / 1000U;
}
