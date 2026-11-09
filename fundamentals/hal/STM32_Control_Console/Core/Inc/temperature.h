/*
 * temperature.h
 *
 *  Created on: Nov 8, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_TEMPERATURE_H_
#define INC_TEMPERATURE_H_

#include "stm32f4xx_hal.h"

/**
 * Read the internal MCU temperature in degrees Celsius.
 * The output is updated only when the reading succeeds.
 */
HAL_StatusTypeDef TemperatureReadCelsius(float *temperature_c);



#endif /* INC_TEMPERATURE_H_ */
