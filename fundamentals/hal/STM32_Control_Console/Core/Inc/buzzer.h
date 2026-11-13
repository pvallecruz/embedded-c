/*
 * buzzer.h
 *
 *  Created on: Nov 13, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_BUZZER_H_
#define INC_BUZZER_H_

#include "stm32f4xx_hal.h"

/* Initialize PWM with the buzzer silent. */
HAL_StatusTypeDef BuzzerInit(void);

/* Start a repeating alarm; repeated calls leave it running. */
void BuzzerAlarmStart(void);

/* Silence the buzzer and cancel the alarm. */
void BuzzerAlarmStop(void);

/* Advance the beep pattern; call regularly from the main loop. */
void BuzzerUpdate(void);

/* Report whether an alarm is active, including silent intervals. */
uint8_t BuzzerAlarmIsActive(void);

#endif /* INC_BUZZER_H_ */
