/*
 * retroscope_ui.h
 *
 *  Created on: Nov 15, 2026
 *      Author: pedrol.valle-cruz
 */

#ifndef INC_RETROSCOPE_UI_H_
#define INC_RETROSCOPE_UI_H_

void RetroScope_DrawLayout(void);
void RetroScope_DrawGrid(void);
void RetroScope_DrawLabels(void);
void RetroScope_DrawWaveform(void);
void RetroScope_DrawWaveformBuffered(void);
void RetroScope_DrawWaveformClipped(void);
void RetroScope_DrawWaveformAdaptive(void);
void RetroScope_DrawWaveformFrame(void);
void RetroScope_GenerateWaveform(float phase_offset);
void RetroScope_LoadADCWaveform(const uint16_t *samples);

#endif /* INC_RETROSCOPE_UI_H_ */
