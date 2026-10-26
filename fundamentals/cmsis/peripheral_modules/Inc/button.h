/*
 * @file   button.h
 * @author Pedro Valle-Cruz
 * @brief  GPIOE button initialization and control.
 * @note   PE10 is active-low with an internal pull-up.
 *				 Call button_init() before controlling the button.
 *				 button_is_pressed() returns the raw state: 1 pressed, 0 released.
 *				 Call button_update(now) regularly with a 32-bit millisecond timestamp.
 *				 It returns 1 once per press accepted after 20 ms of stability, otherwise 0.
 *				 Call it from one polling loop; holding at startup doesn’t count as a press.
 */



#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdint.h>

int button_update(uint32_t now);
void button_init(void);
int button_is_pressed(void);


#endif /* BUTTON_H_ */
