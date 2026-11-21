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

Selectable rising- or falling-edge triggering selects a 401-sample snapshot. The adjustable trigger center is constrained to 64–4031 ADC counts, leaving room for hysteresis thresholds 64 counts below and above it. If no qualifying edge fits the complete display window, the snapshot starts at the beginning of the capture. Frequency detection uses hysteresis at raw ADC counts 1984 and 2112 across the 802-sample capture. Voltage extrema and Vpp use the 401-sample snapshot and an assumed, uncalibrated 3.3 V reference.

The renderer draws at most one strip per main-loop pass so acquisition can be serviced between strips. Frequency and Vpp text update between frames and only redraw when the rounded result changes.

`RequestedTimebaseUs` selects 4000 or 2000 microseconds per division through the debugger. Unsupported values apply the 4 ms/div fallback. The 2 ms/div view repeats samples horizontally; acquisition remains at 10 kHz. Voltage measurements still cover the full snapshot, including samples outside the zoomed view.

## Display controls

Settings are currently changed through debugger expressions; there are no physical controls yet.

| Setting | Supported values | Behavior |
|---|---|---|
| `RequestedTimebaseUs` | `4000`, `2000` | 4 or 2 ms/div; other values use 4 ms/div. |
| `RequestedVerticalScale` | `0`, `1` | Full ADC range (about 0.66 V/div) or 1 V/div; other values use full range. |
| `RequestedTriggerEdge` | `0`, `1` | Rising or falling; other values use rising. |
| `RequestedTriggerLevel` | ADC counts; default `2048` | Effective center is limited to 64–4031; the requested variable is not rewritten. |

The header shows the effective trigger direction and estimated center voltage. A cyan arrow outside the grid marks that center using the current vertical scale. The actual detection thresholds are offset from the center by the hysteresis amount.

`AUTO` indicates automatic fallback operation. `TRIG` means a qualifying edge was found for the displayed snapshot; `FREE` means the fallback window was used. These indicators do not establish calibration or signal quality. Frequency detection keeps its independent, fixed thresholds when the waveform trigger level changes.

## Open and build

Choose a separate CubeIDE workspace, such as `/Users/pedrol.valle-cruz/STM32CubeIDE/workspace_2.1.0`; do not select the project folder as the workspace. Import this directory into STM32CubeIDE as an existing project, leaving “Copy projects into workspace” unchecked. Keep the project name `RetroScope`. The Debug launch uses the project-relative executable `Debug/RetroScope.elf`.

The project has been re-imported into the existing CubeIDE workspace after relocation. If importing into another workspace, select this directory rather than the old `fundamentals/hal/RetroScope` path. Build the Debug configuration before launching. CubeIDE regenerates its build files using the new project location.

## Validation and limits

User hardware checks before relocation confirmed 100 Hz and 200 Hz measurements, constant-low invalid frequency, live PWM-to-constant-to-PWM label recovery, estimated voltage conversion, hysteresis operation with the clean test signal, and 4/2/4 ms/div switching and invalid-setting fallback. Incremental rendering eliminated the observed capture drops in the tested uninterrupted 200 Hz runs. A deliberate debugger pause exercised capture rejection and recovery.

Full-height 200 Hz waveform frames were observed around 117 ms; incremental rendering improves acquisition responsiveness, not the total SPI transfer time. This is not a calibrated instrument: sampling and the test PWM share a clock source, full-scale ADC readings may be saturated, and absolute accuracy and operating limits are not characterized. The board input has no oscilloscope-grade protection or attenuation; the current setup is for controlled board-level experiments.

### Reported hardware checks after relocation

The following were reported passing by the user with the on-board PWM test connection and clean IDE builds:

- Full-range / 1 V/div switching and unsupported vertical-scale fallback.
- Rising / falling / rising trigger selection, plus unsupported edge value `2` falling back to rising.
- Adjustable trigger centers 1024, 2048, and 3072; labels 0.83, 1.65, and 2.48 V.
- Boundary requests 0 and 4095; effective labels 0.05 and 3.25 V. At the upper limit the waveform drifted; reliable endpoint triggering is not claimed.
- Marker movement, clean erasure, and alignment under both vertical scales.
- With trigger center 2048, live TIM3 CCR1 switching 2500 → 0 → 2500 produced square wave → low line → square wave, frequency 200 Hz → unavailable → 200 Hz, and status TRIG → FREE → TRIG.
- Zero unsafe-capture and occupied-buffer drop counts during the reported uninterrupted normal runs.

These checks preserve normal operation; they do not characterize noise rejection, absolute accuracy, all signal shapes, or all sampling conditions.

## Roadmap

First-version milestones:

1. Run/Stop with waveform and displayed measurements held together.
2. Normal and Single trigger modes.
3. Physical controls for settings currently changed through the debugger.

Later milestones: pre-trigger viewing, rendering performance, calibration and input hardware, and additional measurements such as duty cycle, pulse width, average, and RMS.

Project documentation will include focused architecture, sequence, and state diagrams as useful. These are planned; they are not implemented features.

[Back to projects](../README.md)
