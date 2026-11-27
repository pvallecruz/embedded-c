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
#define RETROSCOPE_TIMEBASE_2_MS 2000U
#define RETROSCOPE_TIMEBASE_4_MS 4000U
#define RETROSCOPE_VERTICAL_FULL_RANGE 0U
#define RETROSCOPE_VERTICAL_1_V_DIV 1U
#define RETROSCOPE_SETTING_TIMEBASE 0U
#define RETROSCOPE_SETTING_VERTICAL_SCALE 1U
#define RETROSCOPE_SETTING_TRIGGER_LEVEL 2U
#define RETROSCOPE_SETTING_TRIGGER_EDGE 3U
#define RETROSCOPE_SETTING_COUNT 4U

// Debugger-visible rendering diagnostic.
extern volatile uint16_t StripsDrawn;

void RetroScope_DrawLayout(void);
void RetroScope_DrawGrid(void);
void RetroScope_DrawLabels(void);
void RetroScope_UpdateTimebase(uint32_t TimebaseUs);
void RetroScope_UpdateFrequency(uint32_t FrequencyHz, uint8_t Valid);
void RetroScope_LoadAdcWaveform(const uint16_t* samples, uint32_t TimebaseUs,
                                uint32_t VerticalScale, float ReferenceVolts);
uint8_t RetroScope_IsFrameActive(void);
void RetroScope_BeginFrame(void);
void RetroScope_RenderStep(void);
void RetroScope_UpdateVpp(uint32_t Centivolts, uint8_t Valid);
void RetroScope_UpdateVerticalScale(uint32_t VerticalScale);
void RetroScope_UpdateTriggerEdge(uint8_t FallingEdge);
void RetroScope_UpdateTriggerLevel(uint32_t Centivolts);
void RetroScope_UpdateTriggerMarker(uint32_t TriggerLevel,
                                    uint32_t VerticalScale,
                                    float ReferenceVolts);
void RetroScope_UpdateTriggerStatus(uint8_t Triggered, uint8_t Waiting);
void RetroScope_UpdateRunStatus(uint8_t Running);
void RetroScope_UpdateTriggerMode(uint32_t Mode);
void RetroScope_UpdateSelectedSetting(uint32_t Setting);
void RetroScope_UpdatePendingSettings(uint8_t Pending);

#endif /* INC_RETROSCOPE_UI_H_ */
