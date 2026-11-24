/*
 * retroscope_input.h
 *
 *  Created on: Nov 22, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_RETROSCOPE_INPUT_H_
#define INC_RETROSCOPE_INPUT_H_

#include <stdint.h>

// Call regularly from the main loop.
// Returns 1 once per debounced press, otherwise 0.
uint8_t RetroScope_InputPollRunStop(void);
uint8_t RetroScope_InputPollMode(void);
uint8_t RetroScope_InputPollSelect(void);
uint8_t RetroScope_InputPollDecrease(void);
uint8_t RetroScope_InputPollIncrease(void);



#endif /* INC_RETROSCOPE_INPUT_H_ */
