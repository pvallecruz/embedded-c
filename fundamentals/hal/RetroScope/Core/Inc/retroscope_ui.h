/*
 * retroscope_ui.h
 *
 *  Created on: Nov 15, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_RETROSCOPE_UI_H_
#define INC_RETROSCOPE_UI_H_

#include <stdint.h>

#define RETROSCOPE_WAVEFORM_SAMPLES 401U

// Debugger-visible rendering diagnostic.
extern volatile uint16_t StripsDrawn;

void RetroScope_DrawLayout(void);
void RetroScope_DrawGrid(void);
void RetroScope_DrawLabels(void);
void RetroScope_DrawWaveformFrame(void);
void RetroScope_LoadAdcWaveform(const uint16_t *samples);

#endif /* INC_RETROSCOPE_UI_H_ */
