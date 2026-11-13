/*
 * buzzer.c
 *
 *  Created on: Nov 13, 2026
 *      Author: pedrol.valle-cruz
 */

#include "buzzer.h"
#include "tim.h"

#define BUZZER_ON_MS   200U
#define BUZZER_OFF_MS  800U
#define BUZZER_PULSE   250U

static uint8_t buzzer_ready = 0U;
static uint8_t alarm_active = 0U;
static uint8_t tone_on = 0U;
static uint32_t transition_time = 0U;

HAL_StatusTypeDef BuzzerInit(void) {
  buzzer_ready = 0U;
  alarm_active = 0U;
  tone_on = 0U;

  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0U);

  HAL_StatusTypeDef status =
      HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);

  if (status == HAL_OK) {
    buzzer_ready = 1U;
  }

  return status;
}

void BuzzerAlarmStart(void) {
  if (!buzzer_ready || alarm_active) {
    return;
  }

  alarm_active = 1U;
  tone_on = 1U;
  transition_time = HAL_GetTick();

  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, BUZZER_PULSE);
}

void BuzzerAlarmStop(void) {
  alarm_active = 0U;
  tone_on = 0U;

  if (buzzer_ready) {
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0U);
  }
}

void BuzzerUpdate(void) {
  if (!buzzer_ready || !alarm_active) {
    return;
  }

  uint32_t now = HAL_GetTick();
  uint32_t duration = tone_on ? BUZZER_ON_MS : BUZZER_OFF_MS;

  if ((uint32_t)(now - transition_time) < duration) {
    return;
  }

  transition_time = now;
  tone_on = !tone_on;

  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1,
                       tone_on ? BUZZER_PULSE : 0U);
}

uint8_t BuzzerAlarmIsActive(void) {
  return alarm_active;
}

