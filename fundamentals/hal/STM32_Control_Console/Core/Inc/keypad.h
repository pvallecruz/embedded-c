/*
 * keypad.h
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include "stm32f4xx_hal.h"

char KeypadScan(void);
char KeypadGetKey(void);

#endif /* INC_KEYPAD_H_ */
