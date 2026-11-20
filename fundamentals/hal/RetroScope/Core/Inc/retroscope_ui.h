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
void RetroScope_UpdateFrequency(uint32_t FrequencyHz, uint8_t Valid);
void RetroScope_LoadAdcWaveform(const uint16_t* samples);
uint8_t RetroScope_IsFrameActive(void);
void RetroScope_BeginFrame(void);
void RetroScope_RenderStep(void);
void RetroScope_UpdateVpp(uint32_t Centivolts, uint8_t Valid);

#endif /* INC_RETROSCOPE_UI_H_ */
