/*
 * console.h
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_CONSOLE_H_
#define INC_CONSOLE_H_

#include "stm32f4xx_hal.h"

typedef enum {
  CONSOLE_MAIN_MENU,
  CONSOLE_LED_CONTROL,
  CONSOLE_SYSTEM_STATUS,
  CONSOLE_TIMER_SETTINGS
} ConsoleState;

HAL_StatusTypeDef ConsoleInit(void);
HAL_StatusTypeDef ConsoleUpdate(void);



#endif /* INC_CONSOLE_H_ */
