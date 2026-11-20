# RetroScope

An STM32F407G-DISC1 oscilloscope prototype built in C with STM32 HAL and STM32CubeIDE. This project began as HAL fundamentals practice and now lives under `projects/RetroScope`.

## Hardware and current configuration

- ST7796S 480 × 320 SPI display; SPI1 nominally 24 MHz.
- ADC1 channel 0 on PA0, triggered by TIM2 at nominally 10 kHz.
- DMA2 Stream0, channel 0, circular acquisition: 1,604 samples, two 802-sample halves.
- TIM3 CH1 on PB4 supplies a nominal 200 Hz, 50% duty-cycle test signal.
- Test connection: PB4 through a 1 kΩ resistor to PA0 on the same board.
- Display wiring: PA4 CS, PA5 SCK, PA7 MOSI, PB0 reset, PB1 data/command.

## Architecture

DMA callbacks validate the source half, copy it into an available software capture buffer, and publish it. The main loop processes that protected capture before releasing it. Occupied-buffer drops and rejected unsafe captures have separate counters.

Rising-edge triggering selects a 401-sample snapshot. Frequency detection uses hysteresis at raw ADC counts 1984 and 2112 across the 802-sample capture. Voltage extrema and Vpp use the 401-sample snapshot and an assumed, uncalibrated 3.3 V reference.

The renderer draws at most one strip per main-loop pass so acquisition can be serviced between strips. Frequency and Vpp text update between frames and only redraw when the rounded result changes.

`RequestedTimebaseUs` selects 4000 or 2000 microseconds per division through the debugger. Unsupported values apply the 4 ms/div fallback. The 2 ms/div view repeats samples horizontally; acquisition remains at 10 kHz. Voltage measurements still cover the full snapshot, including samples outside the zoomed view.

## Open and build

Import this directory into STM32CubeIDE as an existing project. Keep the project name `RetroScope`. The Debug launch uses the project-relative executable `Debug/RetroScope.elf`.

The project has been re-imported into the existing CubeIDE workspace after relocation. If importing into another workspace, select this directory rather than the old `fundamentals/hal/RetroScope` path. Build the Debug configuration before launching. CubeIDE regenerates its build files using the new project location.

## Validation and limits

User hardware checks before relocation confirmed 100 Hz and 200 Hz measurements, constant-low invalid frequency, live PWM-to-constant-to-PWM label recovery, estimated voltage conversion, hysteresis operation with the clean test signal, and 4/2/4 ms/div switching and invalid-setting fallback. Incremental rendering eliminated the observed capture drops in the tested uninterrupted 200 Hz runs. A deliberate debugger pause exercised capture rejection and recovery.

Full-height 200 Hz waveform frames were observed around 117 ms; incremental rendering improves acquisition responsiveness, not the total SPI transfer time. This is not a calibrated instrument: sampling and the test PWM share a clock source, full-scale ADC readings may be saturated, and absolute accuracy and operating limits are not characterized. The board input has no oscilloscope-grade protection or attenuation; the current setup is for controlled board-level experiments.

Next work includes selectable vertical scaling, further trigger controls, calibration, and additional measurement validation. Relocation build verification does not replace a post-move hardware/debug smoke test.

[Back to projects](../README.md)
