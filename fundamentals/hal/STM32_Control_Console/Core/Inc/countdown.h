/*
 * countdown.h
 *
 *  Created on: Nov 13, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_COUNTDOWN_H_
#define INC_COUNTDOWN_H_

#include <stdint.h>

typedef enum {
  COUNTDOWN_IDLE,
  COUNTDOWN_RUNNING,
  COUNTDOWN_COMPLETE
} CountdownState;

/* Initialize or reset to idle. */
void CountdownReset(void);

/* Start or restart with 1–999 seconds.
 * Return 1 on success; 0 leaves the current countdown unchanged.
 */
uint8_t CountdownStart(uint32_t seconds);

/* Call regularly. Return 1 only when running changes to complete. */
uint8_t CountdownUpdate(void);

CountdownState CountdownGetState(void);

/* Whole seconds remaining, rounded up; zero when idle or complete. */
uint32_t CountdownGetRemainingSeconds(void);


#endif /* INC_COUNTDOWN_H_ */
