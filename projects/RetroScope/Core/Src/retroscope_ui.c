/*
 * retroscope_ui.c
 *
 *  Created on: Nov 15, 2026
 *      Author: pedrol.valle-cruz
 */

#include "retroscope_ui.h"

#include <stdint.h>
#include <stdio.h>

#include "st7796.h"

// Screen regions
#define HEADER_HEIGHT 34U
#define WAVEFORM_HEIGHT 224U
#define INFO_HEIGHT 62U

#define WAVEFORM_Y HEADER_HEIGHT
#define INFO_Y (HEADER_HEIGHT + WAVEFORM_HEIGHT)

// Interface colors (RGB565)
#define UI_BACKGROUND 0x0000U
#define UI_PANEL_COLOR 0x1948U
#define UI_WAVE_COLOR 0x0841U
#define UI_BORDER_COLOR 0x4A69U
#define UI_WAVEFORM_COLOR 0xFFE0U

// Graticule geometry
#define GRID_X 40U
#define GRID_Y 46U
#define GRID_WIDTH 400U
#define GRID_HEIGHT 200U
#define GRID_SPACING 40U

// Graticule colors (RGB565)
#define GRID_COLOR 0x2B4AU
#define GRID_AXIS_COLOR 0x5EAFU

// Waveform geometry
#define WAVEFORM_SAMPLES RETROSCOPE_WAVEFORM_SAMPLES

// Rendering buffer
#define RENDER_STRIP_WIDTH 20U
#define RENDER_BUFFER_WIDTH (RENDER_STRIP_WIDTH + 1U)
#define RENDER_BUFFER_SIZE (RENDER_BUFFER_WIDTH * (GRID_HEIGHT + 1U))

static uint16_t Waveform[WAVEFORM_SAMPLES];
static uint16_t RenderBuffer[RENDER_BUFFER_SIZE];
static uint16_t PreviousWaveform[WAVEFORM_SAMPLES];
static uint8_t PreviousWaveformValid = 0U;
static uint16_t NextStripX = GRID_X;
static uint8_t FrameActive = 0U;

volatile uint16_t StripsDrawn = 0U;

void RetroScope_DrawLayout(void) {
  // Clear entire screen
  ST7796_FillScreen(UI_BACKGROUND);

  // Header
  ST7796_FillRect(0, 0, ST7796_WIDTH, HEADER_HEIGHT, UI_PANEL_COLOR);

  // Waveform viewport
  ST7796_FillRect(0, WAVEFORM_Y, ST7796_WIDTH, WAVEFORM_HEIGHT, UI_WAVE_COLOR);

  // Bottom information bar
  ST7796_FillRect(0, INFO_Y, ST7796_WIDTH, INFO_HEIGHT, UI_PANEL_COLOR);

  // Separators
  ST7796_DrawLine(0, WAVEFORM_Y, ST7796_WIDTH - 1, WAVEFORM_Y, UI_BORDER_COLOR);

  ST7796_DrawLine(0, INFO_Y, ST7796_WIDTH - 1, INFO_Y, UI_BORDER_COLOR);
}

void RetroScope_DrawGrid(void) {
  const uint16_t x_end = GRID_X + GRID_WIDTH;
  const uint16_t y_end = GRID_Y + GRID_HEIGHT;

  const uint16_t x_center = GRID_X + GRID_WIDTH / 2;
  const uint16_t y_center = GRID_Y + GRID_HEIGHT / 2;

  // Draw vertical grid lines
  for (uint16_t x = GRID_X; x <= x_end; x += GRID_SPACING) {
    ST7796_DrawLine(x, GRID_Y, x, y_end, GRID_COLOR);
  }

  // Draw horizontal grid lines
  for (uint16_t y = GRID_Y; y <= y_end; y += GRID_SPACING) {
    ST7796_DrawLine(GRID_X, y, x_end, y, GRID_COLOR);
  }

  // Draw brighter vertical reference axis
  ST7796_DrawLine(x_center, GRID_Y, x_center, y_end, GRID_AXIS_COLOR);

  // Draw brighter horizontal reference axis
  ST7796_DrawLine(GRID_X, y_center, x_end, y_center, GRID_AXIS_COLOR);
}

void RetroScope_DrawLabels(void) {
  // Header title
  ST7796_DrawString(12, 10, "RETROSCOPE", ST7796_WHITE, 2);

  // Default trigger direction.
  ST7796_DrawString(180, 10, "TRIG: RISE", ST7796_WHITE, 2);

  // Trigger center voltage; unavailable before the first capture.
  ST7796_DrawString(306, 14, "-- V", ST7796_CYAN, 1);

  // Header status indicators
  ST7796_DrawString(360, 10, "RUN", ST7796_GREEN, 2);

  ST7796_DrawString(420, 10, "AUTO", ST7796_WHITE, 1);

  // Capture status is unavailable until the first snapshot.
  ST7796_DrawString(420, 22, "--", ST7796_WHITE, 1);

  // Bottom information panel
  ST7796_DrawString(15, 273, "CH1", ST7796_GREEN, 2);

  // Fixed scales for the current ADC mapping and sampling configuration.
  ST7796_DrawString(110, 273, "0.66 V/DIV", ST7796_WHITE, 2);
  ST7796_DrawString(250, 273, "4.00 MS/DIV", ST7796_WHITE, 2);

  // Frequency remains unavailable until a valid measurement arrives.
  ST7796_DrawString(15, 298, "FREQ: -- HZ", ST7796_WHITE, 2);
  // Estimated peak-to-peak voltage; unavailable before the first capture.
  ST7796_DrawString(260, 298, "VPP: -- V", ST7796_WHITE, 2);
}

void RetroScope_UpdateTriggerStatus(uint8_t Triggered) {
  static uint8_t PreviousTriggered = 0U;
  static uint8_t DisplayInitialized = 0U;

  if (FrameActive != 0U) {
    return;
  }

  Triggered = (Triggered != 0U) ? 1U : 0U;

  if (DisplayInitialized != 0U &&
      Triggered == PreviousTriggered) {
    return;
  }

  const char *Text = (Triggered != 0U) ? "TRIG" : "FREE";
  uint16_t Color = (Triggered != 0U) ? ST7796_GREEN : ST7796_WHITE;

  ST7796_FillRect(420, 22, 24, 7, UI_PANEL_COLOR);
  ST7796_DrawString(420, 22, Text, Color, 1);

  PreviousTriggered = Triggered;
  DisplayInitialized = 1U;
}

void RetroScope_UpdateTriggerEdge(uint8_t FallingEdge) {
  static uint8_t PreviousFallingEdge = 0U;
  static uint8_t DisplayInitialized = 0U;

  if (FrameActive != 0U) {
    return;
  }

  FallingEdge = (FallingEdge != 0U) ? 1U : 0U;

  if (DisplayInitialized != 0U && FallingEdge == PreviousFallingEdge) {
    return;
  }

  const char* Text = (FallingEdge != 0U) ? "FALL" : "RISE";

  // Preserve "TRIG: " and replace only the direction.
  ST7796_FillRect(252, 10, 48, 14, UI_PANEL_COLOR);
  ST7796_DrawString(252, 10, Text, ST7796_CYAN, 2);

  PreviousFallingEdge = FallingEdge;
  DisplayInitialized = 1U;
}

void RetroScope_UpdateTriggerLevel(uint32_t Centivolts) {
  static uint32_t PreviousCentivolts = 0U;
  static uint8_t DisplayInitialized = 0U;

  if (FrameActive != 0U) {
    return;
  }

  if (DisplayInitialized != 0U && Centivolts == PreviousCentivolts) {
    return;
  }

  char Text[12];

  // Limit formatting to values that fit the display field.
  if (Centivolts <= 999U) {
    snprintf(Text, sizeof(Text), "%lu.%02lu V",
             (unsigned long)(Centivolts / 100U),
             (unsigned long)(Centivolts % 100U));
  } else {
    snprintf(Text, sizeof(Text), "-- V");
  }

  ST7796_FillRect(306, 14, 36, 7, UI_PANEL_COLOR);
  ST7796_DrawString(306, 14, Text, ST7796_CYAN, 1);

  PreviousCentivolts = Centivolts;
  DisplayInitialized = 1U;
}

void RetroScope_UpdateTriggerMarker(uint32_t TriggerLevel,
                                    uint32_t VerticalScale,
                                    float ReferenceVolts) {
  static uint16_t PreviousY = 0U;
  static uint8_t MarkerInitialized = 0U;

  if (FrameActive != 0U) {
    return;
  }

  if (TriggerLevel > 4095U) {
    TriggerLevel = 4095U;
  }

  uint32_t Scaled;

  if (VerticalScale == RETROSCOPE_VERTICAL_1_V_DIV) {
    float Pixels =
        ((float)TriggerLevel / 4095.0f) * ReferenceVolts * GRID_SPACING;

    if (!(Pixels >= 0.0f)) {
      Pixels = 0.0f;
    } else if (Pixels > (float)(GRID_HEIGHT - 1U)) {
      Pixels = (float)(GRID_HEIGHT - 1U);
    }

    Scaled = (uint32_t)Pixels;
  } else {
    Scaled = (TriggerLevel * (GRID_HEIGHT - 1U)) / 4095U;
  }

  uint16_t MarkerY = GRID_Y + GRID_HEIGHT - 1U - (uint16_t)Scaled;

  if (MarkerInitialized != 0U && MarkerY == PreviousY) {
    return;
  }

  // Erase the old marker in the margin outside the grid.
  if (MarkerInitialized != 0U) {
    ST7796_FillRect(GRID_X - 8U, PreviousY - 3U, 7U, 7U, UI_WAVE_COLOR);
  }

  // Draw a right-pointing arrow without touching the grid.
  ST7796_DrawLine(GRID_X - 8U, MarkerY, GRID_X - 2U, MarkerY, ST7796_CYAN);
  ST7796_DrawLine(GRID_X - 5U, MarkerY - 3U, GRID_X - 2U, MarkerY, ST7796_CYAN);
  ST7796_DrawLine(GRID_X - 5U, MarkerY + 3U, GRID_X - 2U, MarkerY, ST7796_CYAN);

  PreviousY = MarkerY;
  MarkerInitialized = 1U;
}

void RetroScope_UpdateTimebase(uint32_t TimebaseUs) {
  static uint32_t PreviousTimebaseUs = 0U;

  if (FrameActive != 0U) {
    return;
  }

  // Unsupported settings fall back to the existing 4 ms/div scale.
  if (TimebaseUs != RETROSCOPE_TIMEBASE_2_MS) {
    TimebaseUs = RETROSCOPE_TIMEBASE_4_MS;
  }

  if (TimebaseUs == PreviousTimebaseUs) {
    return;
  }

  const char* Text =
      (TimebaseUs == RETROSCOPE_TIMEBASE_2_MS) ? "2.00 MS/DIV" : "4.00 MS/DIV";

  ST7796_FillRect(250, 273, 132, 14, UI_PANEL_COLOR);
  ST7796_DrawString(250, 273, Text, ST7796_WHITE, 2);

  PreviousTimebaseUs = TimebaseUs;
}

void RetroScope_UpdateVerticalScale(uint32_t VerticalScale) {
  static uint32_t PreviousScale = RETROSCOPE_VERTICAL_FULL_RANGE;
  static uint8_t DisplayInitialized = 0U;

  if (FrameActive != 0U) {
    return;
  }

  if (VerticalScale != RETROSCOPE_VERTICAL_1_V_DIV) {
    VerticalScale = RETROSCOPE_VERTICAL_FULL_RANGE;
  }

  if (DisplayInitialized != 0U && VerticalScale == PreviousScale) {
    return;
  }

  const char* Text = (VerticalScale == RETROSCOPE_VERTICAL_1_V_DIV)
                         ? "1.00 V/DIV"
                         : "0.66 V/DIV";

  ST7796_FillRect(110, 273, 120, 14, UI_PANEL_COLOR);
  ST7796_DrawString(110, 273, Text, ST7796_WHITE, 2);

  PreviousScale = VerticalScale;
  DisplayInitialized = 1U;
}

void RetroScope_UpdateFrequency(uint32_t FrequencyHz, uint8_t Valid) {
  static uint32_t PreviousHz = 0U;
  static uint8_t PreviousValid = 0U;
  static uint8_t DisplayInitialized = 0U;

  Valid = (Valid != 0U);

  if (DisplayInitialized != 0U && Valid == PreviousValid &&
      (Valid == 0U || FrequencyHz == PreviousHz)) {
    return;
  }

  char Text[16];

  if (Valid != 0U) {
    snprintf(Text, sizeof(Text), "%lu HZ", (unsigned long)FrequencyHz);
  } else {
    snprintf(Text, sizeof(Text), "-- HZ");
  }

  // Preserve "FREQ: " and replace only the value and units.
  ST7796_FillRect(87, 298, 160, 14, UI_PANEL_COLOR);
  ST7796_DrawString(87, 298, Text, ST7796_CYAN, 2);

  PreviousHz = FrequencyHz;
  PreviousValid = Valid;
  DisplayInitialized = 1U;
}

void RetroScope_UpdateVpp(uint32_t Centivolts, uint8_t Valid) {
  static uint32_t PreviousCentivolts = 0U;
  static uint8_t PreviousValid = 0U;
  static uint8_t DisplayInitialized = 0U;

  // Limit formatting to values that fit the display field.
  Valid = (Valid != 0U && Centivolts <= 999U);

  if (DisplayInitialized != 0U && Valid == PreviousValid &&
      (Valid == 0U || Centivolts == PreviousCentivolts)) {
    return;
  }

  char Text[12];

  if (Valid != 0U) {
    snprintf(Text, sizeof(Text), "%lu.%02lu V",
             (unsigned long)(Centivolts / 100U),
             (unsigned long)(Centivolts % 100U));
  } else {
    snprintf(Text, sizeof(Text), "-- V");
  }

  // Preserve "VPP: " and replace only the value and units.
  ST7796_FillRect(320, 298, 96, 14, UI_PANEL_COLOR);
  ST7796_DrawString(320, 298, Text, ST7796_CYAN, 2);

  PreviousCentivolts = Centivolts;
  PreviousValid = Valid;
  DisplayInitialized = 1U;
}

static void RetroScope_GetStripBounds(uint16_t start_x, uint16_t* top,
                                      uint16_t* bottom) {
  uint16_t first = start_x - GRID_X;

  uint16_t min_y = Waveform[first];
  uint16_t max_y = Waveform[first];

  for (uint16_t i = 0; i <= RENDER_STRIP_WIDTH; i++) {
    uint16_t index = first + i;
    uint16_t current_y = Waveform[index];

    if (current_y < min_y) min_y = current_y;
    if (current_y > max_y) max_y = current_y;

    if (PreviousWaveformValid) {
      uint16_t previous_y = PreviousWaveform[index];

      if (previous_y < min_y) min_y = previous_y;
      if (previous_y > max_y) max_y = previous_y;
    }
  }

  const uint16_t grid_bottom = GRID_Y + GRID_HEIGHT - 1U;

  if (min_y < GRID_Y) {
    min_y = GRID_Y;
  }

  if (max_y > grid_bottom) {
    max_y = grid_bottom;
  }

  *top = (min_y > GRID_Y) ? min_y - 1U : GRID_Y;

  *bottom = (max_y < grid_bottom) ? max_y + 1U : grid_bottom;
}

static uint8_t RetroScope_StripChanged(uint16_t start_x) {
  if (!PreviousWaveformValid) {
    return 1U;
  }

  uint16_t first = start_x - GRID_X;

  // Compare all 21 samples belonging to this strip.
  for (uint16_t i = 0; i <= RENDER_STRIP_WIDTH; i++) {
    uint16_t index = first + i;

    if (Waveform[index] != PreviousWaveform[index]) {
      return 1U;
    }
  }

  return 0U;
}

static void RetroScope_DrawWaveformStrip(uint16_t start_x) {
  uint16_t strip_top;
  uint16_t strip_bottom;

  RetroScope_GetStripBounds(start_x, &strip_top, &strip_bottom);

  uint16_t strip_height = strip_bottom - strip_top + 1U;

  uint16_t strip_width = RENDER_STRIP_WIDTH;

  // Final strip includes the rightmost endpoint.
  if (start_x + strip_width == GRID_X + GRID_WIDTH) {
    strip_width++;
  }

  // Prepare background and grid.
  for (uint16_t y = 0; y < strip_height; y++) {
    uint16_t screen_y = strip_top + y;

    for (uint16_t x = 0; x < strip_width; x++) {
      uint16_t screen_x = start_x + x;
      uint16_t color = UI_WAVE_COLOR;

      if ((screen_x - GRID_X) % GRID_SPACING == 0U ||
          (screen_y - GRID_Y) % GRID_SPACING == 0U) {
        color = GRID_COLOR;
      }

      if (screen_x == GRID_X + GRID_WIDTH / 2U ||
          screen_y == GRID_Y + GRID_HEIGHT / 2U) {
        color = GRID_AXIS_COLOR;
      }

      RenderBuffer[y * RENDER_BUFFER_WIDTH + x] = color;
    }
  }

  // Draw the current waveform.
  uint16_t first_sample = start_x - GRID_X;

  for (uint16_t x = 0; x < RENDER_STRIP_WIDTH; x++) {
    uint16_t index = first_sample + x;

    uint16_t y0 = Waveform[index];
    uint16_t y1 = Waveform[index + 1U];

    uint16_t min_y = (y0 < y1) ? y0 : y1;
    uint16_t max_y = (y0 > y1) ? y0 : y1;

    for (uint16_t y = min_y; y <= max_y; y++) {
      if (y >= strip_top && y <= strip_bottom) {
        uint16_t local_y = y - strip_top;

        RenderBuffer[local_y * RENDER_BUFFER_WIDTH + x] = UI_WAVEFORM_COLOR;
      }
    }
  }

  // Draw the last endpoint in the final strip.
  if (strip_width > RENDER_STRIP_WIDTH) {
    uint16_t final_y = Waveform[WAVEFORM_SAMPLES - 1U];

    if (final_y >= strip_top && final_y <= strip_bottom) {
      RenderBuffer[(final_y - strip_top) * RENDER_BUFFER_WIDTH +
                   RENDER_STRIP_WIDTH] = UI_WAVEFORM_COLOR;
    }
  }

  // Our buffer uses a 21-pixel row stride.
  // Pack active rows to match the LCD transfer width.
  for (uint16_t y = 1; y < strip_height; y++) {
    for (uint16_t x = 0; x < strip_width; x++) {
      RenderBuffer[y * strip_width + x] =
          RenderBuffer[y * RENDER_BUFFER_WIDTH + x];
    }
  }

  ST7796_DrawRGB565Buffer(start_x, strip_top, strip_width, strip_height,
                          RenderBuffer);
}

uint8_t RetroScope_IsFrameActive(void) { return FrameActive; }

void RetroScope_BeginFrame(void) {
  if (FrameActive != 0U) {
    return;
  }

  NextStripX = GRID_X;
  StripsDrawn = 0U;
  FrameActive = 1U;
}

void RetroScope_RenderStep(void) {
  if (FrameActive == 0U) {
    return;
  }

  if (RetroScope_StripChanged(NextStripX)) {
    RetroScope_DrawWaveformStrip(NextStripX);
    StripsDrawn++;
  }

  NextStripX += RENDER_STRIP_WIDTH;

  if (NextStripX >= GRID_X + GRID_WIDTH) {
    for (uint16_t i = 0U; i < WAVEFORM_SAMPLES; i++) {
      PreviousWaveform[i] = Waveform[i];
    }

    PreviousWaveformValid = 1U;
    FrameActive = 0U;
  }
}

void RetroScope_LoadAdcWaveform(const uint16_t* samples, uint32_t TimebaseUs,
                                uint32_t VerticalScale, float ReferenceVolts) {
  if (samples == NULL || FrameActive != 0U) {
    return;
  }

  uint16_t SampleDivisor = (TimebaseUs == RETROSCOPE_TIMEBASE_2_MS) ? 2U : 1U;

  for (uint16_t i = 0U; i < WAVEFORM_SAMPLES; i++) {
    uint16_t adc = samples[i / SampleDivisor];

    if (adc > 4095U) {
      adc = 4095U;
    }

    uint32_t scaled;

    if (VerticalScale == RETROSCOPE_VERTICAL_1_V_DIV) {
      float Pixels = ((float)adc / 4095.0f) * ReferenceVolts * GRID_SPACING;

      // Bound the coordinate before converting to an integer.
      if (!(Pixels >= 0.0f)) {
        Pixels = 0.0f;
      } else if (Pixels > (float)(GRID_HEIGHT - 1U)) {
        Pixels = (float)(GRID_HEIGHT - 1U);
      }

      scaled = (uint32_t)Pixels;
    } else {
      // Preserve the original full-range mapping.
      scaled = ((uint32_t)adc * (GRID_HEIGHT - 1U)) / 4095U;
    }

    Waveform[i] = GRID_Y + GRID_HEIGHT - 1U - (uint16_t)scaled;
  }
}
