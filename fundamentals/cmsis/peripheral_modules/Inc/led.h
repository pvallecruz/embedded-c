/*
 * @file   led.h
 * @author Pedro Valle-Cruz
 * @brief  GPIOE LED initialization and control.
 * @note   Pass pin numbers (7, 8, 9, or 11), not bit masks.
 *         Call led_init(pin) before controlling that LED.
 *         Initialization leaves the LED off.
 *         Unsupported pins are ignored; functions return no value.
 *         Assumes exclusive control of the selected LED outputs.
 */

#ifndef LED_H_

#define LED_H_
void led_init(unsigned int pin);
void led_on(unsigned int pin);
void led_off(unsigned int pin);
void led_toggle(unsigned int pin);
#endif /* LED_H_ */
