# RetroScope

RetroScope is an STM32F407G-DISC1 oscilloscope prototype built in C with STM32 HAL and STM32CubeIDE. The project began as HAL fundamentals practice and now lives under `projects/RetroScope`.

## Hardware and current configuration

- ST7796S 480 × 320 SPI display; SPI1 nominally 24 MHz.
- ADC1 channel 0 on PA0, triggered by TIM2 at nominally 10 kHz.
- DMA2 Stream0, channel 0, circular acquisition: 1,604 samples, two 802-sample halves.
- TIM3 CH1 on PB4 supplies a nominal 200 Hz, 50% duty-cycle test signal.
- Test connection: PB4 through a 1 kΩ resistor to PA0 on the same board.
- Display wiring: PA4 CS, PA5 SCK, PA7 MOSI, PB0 reset, PB1 data/command.
- Run/Stop pushbutton: PE7 to GND, generated label `RUN_STOP_BTN`.
- Mode pushbutton: PE8 to GND, generated label `MODE_BTN`.
- SELECT pushbutton: PE9 to GND, generated label `SELECT_BTN`.
- Decrease (−) pushbutton: PE10 to GND, generated label `DECREASE_BTN`.
- Increase (+) pushbutton: PE11 to GND, generated label `INCREASE_BTN`.
- All five buttons are normally open inputs with internal pull-ups: released = high, pressed = low. They share board GND; no external pull-up is required for this setup. Use contacts that connect only when pressed, and disconnect power before changing wiring.

## Architecture

DMA callbacks validate the source half, copy it into an available software capture buffer, and publish it. The main loop processes that protected capture before releasing it. Occupied-buffer drops and rejected unsafe captures have separate counters.

Selectable rising- or falling-edge triggering selects a 401-sample snapshot. The adjustable trigger center is constrained to 64–4031 ADC counts, leaving room for hysteresis thresholds 64 counts below and above it. Auto mode uses the beginning of the capture when no qualifying edge fits the complete display window. Normal mode publishes only triggered snapshots. Single mode publishes one triggered snapshot per accepted arm request and protects it until its frame finishes. Frequency detection uses hysteresis at raw ADC counts 1984 and 2112 across the 802-sample capture. Voltage extrema and Vpp use the 401-sample snapshot and an assumed, uncalibrated 3.3 V reference.

The renderer draws at most one strip per main-loop pass so acquisition can be serviced between strips. Frequency and Vpp text update between frames and only redraw when the rounded result changes. Frequency is saved with the accepted snapshot and copied with the waveform selected for display; live ADC frequency diagnostics may therefore differ from the held LCD reading. Single forces a measurement-label update after its frame completes, before automatically stopping.

`RequestedTimebaseUs` selects 4000 or 2000 microseconds per division through the TIME controls or debugger. Unsupported values apply the 4 ms/div fallback. The 2 ms/div view repeats samples horizontally; acquisition remains at 10 kHz. Voltage measurements still cover the full snapshot, including samples outside the zoomed view.

## Display controls

Five physical buttons control Run/Stop, Single arming/cancellation, trigger mode, timebase, vertical scale, trigger level, and trigger edge. Debugger expressions remain available for diagnostics and controlled tests.

### Physical buttons

| Button | Context | Action on one press |
|---|---|---|
| PE7 Run/Stop | Auto or Normal | Toggle display Run/Stop. |
| PE7 Run/Stop | Single idle | Arm one capture; accepted arming resumes the display. |
| PE7 Run/Stop | Single armed, captured, or rendering | Request Stop and cancel the unfinished Single operation; an active frame still finishes. |
| PE8 Mode | Any mode | Cycle AUTO → NORM → SNGL → AUTO, preserving the requested Run/Stop state. |
| PE9 SELECT | Running or stopped | Cycle TIME → VOLTS → LEVEL → EDGE → TIME. |
| PE10 Decrease (−) | Selected setting | Apply the decrease action listed below. |
| PE11 Increase (+) | Selected setting | Apply the increase action listed below. |

Selecting Single does not arm it. Press Run/Stop separately to arm. Changing modes clears any pending arm request, and leaving Single cancels an unfinished Single operation. After a Single capture automatically stops, use Mode to select Auto or Normal, then Run/Stop to resume continuous display updates.

The mode label updates between frames even while stopped; the waveform and measurement labels remain held. If both debounced press events arrive in the same main-loop pass, Mode takes priority and the Run/Stop event is consumed without action. This is a same-pass rule, not a general simultaneous-button or chord detector.

### Shared setting adjustment

The header's `SEL:` indicator identifies the setting controlled by −/+. TIME is selected at startup. Selection changes do not alter the setting values.

| Selection | Decrease (−) | Increase (+) |
|---|---|---|
| TIME | Set 2 ms/div. | Set 4 ms/div. |
| VOLTS | Set full range, approximately 0.66 V/div. | Set 1.00 V/div. |
| LEVEL | Subtract up to 128 ADC counts, stopping at 64. | Add up to 128 ADC counts, stopping at 4031. |
| EDGE | Select rising-edge triggering. | Select falling-edge triggering. |

Repeated TIME, VOLTS, or EDGE actions at the selected endpoint leave the value unchanged. LEVEL steps correspond to approximately 0.103 V with the assumed 3.3 V reference. Values written through the debugger are constrained to 64–4031 before a button step is applied; a partial final step lands exactly on the limit without wrapping. At the upper limit, the high hysteresis threshold reaches 4095, and triggering can become unstable if samples do not reliably reach full scale.

If SELECT and an adjustment event arrive in the same main-loop pass, the selection advances and the adjustment is ignored. If both adjustment events arrive in the same pass, neither is applied. These rules do not guarantee suppression of physical button combinations whose debounced events arrive in different passes.

While stopped, SELECT remains responsive and −/+ update requested settings, but the waveform, scale labels, trigger direction/voltage, and marker continue describing the held frame. Changes become visible when a new frame is displayed. In Single mode, rearm to apply pending changes to the next capture. In Normal mode, a new qualifying capture is required. The `PEND` indicator identifies requested settings that differ from those used for the held frame.

### Pending-settings indicator

`PEND` appears beside `SEL:` when any effective requested timebase, vertical scale, trigger edge, or trigger-center level differs from the corresponding setting recorded for the displayed frame. It applies to all four settings, not only the currently selected one. Changing SELECT alone does not create a pending change. Run/Stop and trigger mode are not part of this comparison.

The comparison uses the same fallback and limiting rules as acquisition and rendering: unsupported timebases use 4 ms/div, unsupported vertical scales use full range, unsupported edges use rising, and trigger centers are constrained to 64–4031 ADC counts. A raw debugger value that resolves to the displayed setting does not create a false pending indication.

- Before the first display frame, PEND remains blank because no frame settings are available for comparison.
- While stopped or waiting in Normal mode, a changed setting can remain pending while the waveform and its labels stay held.
- Restoring all effective requests to the held frame's settings clears PEND without requiring another capture.
- After a new frame applies the requests, PEND clears at the next between-frame update. Single capture refreshes the waveform and measurements before automatically stopping; the indicator then reflects that completed capture.

Display settings are recorded when a frame starts, and the indicator updates only between frames. It does not mark a partially rendered frame as complete or indicate that a trigger has been found; trigger status is shown separately.

### Input implementation

- [`Core/Src/gpio.c`](Core/Src/gpio.c): CubeMX-generated PE7–PE11 input and pull-up configuration.
- [`Core/Src/retroscope_input.c`](Core/Src/retroscope_input.c) and [`Core/Inc/retroscope_input.h`](Core/Inc/retroscope_input.h): main-loop polling through one shared debounce helper, with a separate `RetroScope_ButtonState` instance for each button.
- [`Core/Src/main.c`](Core/Src/main.c): maps press events to Run/Stop, Single, mode selection, setting selection, and bounded adjustments.

Each input accepts a transition after its sampled reading has remained unchanged for at least 30 ms. Both press and release are debounced. Only an accepted low transition generates a press event; holding a button does not repeat, and releasing does not generate an action. The first poll adopts the current state without an event, so a button held through startup must be released and pressed again to act. No button interrupt or blocking debounce delay is used. Response time also depends on main-loop polling and completion of an active frame; 30 ms is not a guaranteed end-to-end latency.

### Debugger settings

Debugger controls remain available for inspection and controlled testing alongside the physical controls.

| Setting | Supported values | Behavior |
|---|---|---|
| `RequestedRun` | `0`, nonzero | Stop or run display updates; acquisition continues. |
| `RequestedTriggerMode` | `0`, `1`, `2` | Auto, Normal, Single; unsupported values (for example `3`) use Auto. |
| `RequestedSingleArm` | Write `1` | Request a Single capture; main clears the request when handled. |
| `RequestedTimebaseUs` | `4000`, `2000` | 4 or 2 ms/div; other values use 4 ms/div. |
| `RequestedVerticalScale` | `0`, `1` | Full ADC range (about 0.66 V/div) or 1 V/div; other values use full range. |
| `RequestedTriggerEdge` | `0`, `1` | Rising or falling; other values use rising. |
| `RequestedTriggerLevel` | ADC counts; default `2048` | Acquisition constrains the effective center to 64–4031 without rewriting the request; LEVEL button actions normalize and update the request. |

The header shows the effective trigger direction and estimated center voltage. A cyan arrow outside the grid marks that center using the current vertical scale. The actual detection thresholds are offset from the center by the hysteresis amount.

The mode label shows `AUTO`, `NORM`, or `SNGL`. `TRIG` means a qualifying edge was found for the displayed snapshot; `FREE` means Auto used the fallback window. `WAIT` indicates that Normal or armed Single is waiting for a qualifying capture while the previous waveform remains visible. Green `RUN` and red `STOP` describe display updates, not whether the ADC/DMA is running. These indicators do not establish calibration or signal quality. Frequency detection keeps its independent, fixed thresholds when the waveform trigger level changes.

### Run/Stop and Normal mode

Setting `RequestedRun = 0` lets an active frame finish and holds the display. Acquisition and processing continue. Setting it back to `1` resumes display updates. In Normal mode, no new snapshot is published without a qualifying trigger, so the previous waveform and its selected measurements remain available while `WAIT` is shown.

A final triggered capture can contain pulses followed by a flat tail when PWM is disabled partway through acquisition. Because processing continues during Stop, that capture may remain pending and appear on resume. A stationary pulse-and-tail trace under `NORM / WAIT` is consistent with holding this final capture; it is not evidence of Auto fallback. Its frequency describes detected edges in the saved 802-sample capture, not the present constant-low input.

### Single capture

1. Select `SNGL` with Mode, or set `RequestedTriggerMode = 2`.
2. Press Run/Stop while Single is idle, or write `RequestedSingleArm = 1`. At a frame boundary, an accepted request skips older pending display snapshots, arms Single, and sets `RequestedRun = 1`.
3. With no qualifying trigger, the previous waveform remains visible under `SNGL / WAIT`.
4. One qualifying capture is published and protected from subsequent Single captures. Its frame is drawn and its measurement labels are refreshed, then `RequestedRun` becomes `0` and the header shows `STOP`.
5. Press Run/Stop again, or write `RequestedSingleArm = 1`, to rearm. Setting Run alone does not arm Single.

The internal sequence is IDLE → ARMED → CAPTURED → RENDERING → IDLE. An arm request is accepted only if Single is idle when the request is handled. Requests are handled between frames; a write during rendering may remain pending until that frame finishes. Rapid or repeated arm writes have not yet been validated on hardware.

Setting `RequestedRun = 0` or leaving Single mode cancels an unfinished Single operation. Pending undisplayed snapshots are skipped; an already active frame still finishes. After automatic Stop, selecting another mode does not itself resume the display: set `RequestedRun = 1` as needed.

## Open and build

Choose a separate CubeIDE workspace, such as `/Users/pedrol.valle-cruz/STM32CubeIDE/workspace_2.1.0`; do not select the project folder as the workspace. Import this directory into STM32CubeIDE as an existing project, leaving “Copy projects into workspace” unchecked. Keep the project name `RetroScope`. The Debug launch uses the project-relative executable `Debug/RetroScope.elf`.

The project has been re-imported into the existing CubeIDE workspace after relocation. If importing into another workspace, select this directory rather than the old `fundamentals/hal/RetroScope` path. Build the Debug configuration before launching. CubeIDE regenerates its build files using the new project location.

## Validation and limits

Hardware checks before relocation confirmed 100 Hz and 200 Hz measurements, constant-low invalid frequency, live PWM-to-constant-to-PWM label recovery, estimated voltage conversion, hysteresis operation with the clean test signal, and 4/2/4 ms/div switching and invalid-setting fallback. Incremental rendering eliminated the previously observed capture drops during uninterrupted 200 Hz tests. A deliberate debugger pause exercised capture rejection and recovery.

Full-height 200 Hz waveform frames were measured around 117 ms; incremental rendering improves acquisition responsiveness, not the total SPI transfer time. This is not a calibrated instrument: sampling and the test PWM share a clock source, full-scale ADC readings may be saturated, and absolute accuracy and operating limits are not characterized. The board input has no oscilloscope-grade protection or attenuation; the current setup is for controlled board-level experiments.

### Hardware checks after relocation

The following checks passed with the on-board PWM test connection and clean IDE builds:

- Full-range / 1 V/div switching and unsupported vertical-scale fallback.
- Rising / falling / rising trigger selection, plus unsupported edge value `2` falling back to rising.
- Adjustable trigger centers 1024, 2048, and 3072; labels 0.83, 1.65, and 2.48 V.
- Boundary requests 0 and 4095; effective labels 0.05 and 3.25 V. At the upper limit the waveform drifted; reliable endpoint triggering is not claimed.
- Marker movement, clean erasure, and alignment under both vertical scales.
- With trigger center 2048, live TIM3 CCR1 switching 2500 → 0 → 2500 produced square wave → low line → square wave, frequency 200 Hz → unavailable → 200 Hz, and status TRIG → FREE → TRIG.
- Zero unsafe-capture and occupied-buffer drop counts during uninterrupted normal runs.

### Run/Stop and trigger-mode checks (2026-11-22 checkpoint)

Hardware testing confirmed the following behaviors:

| Check | Observed result |
|---|---|
| Run/Stop with PWM changed while stopped | Display and labels held; resuming updated them; RUN/STOP text switched cleanly. |
| Normal, PWM 2500 → 0 → 2500 in TIM3 CCR1 | TRIG → WAIT → TRIG; held waveform and measurements recovered when PWM returned. |
| Normal with Stop and resume after PWM removal | Final pulse-and-flat-tail capture remained stationary under WAIT; restoring PWM resumed full square-wave updates. |
| Single with PWM present | One capture and its measurements displayed, then automatic STOP. |
| Single armed with CCR1 = 0 | WAIT persisted; restoring CCR1 = 2500 produced a capture and automatic STOP. |
| Cancel armed Single with Run = 0 | Display stayed stopped after PWM returned; explicit rearm captured successfully. |
| Leave Single while waiting | Auto resumed fallback display without an unintended automatic stop. |
| Unsupported mode value 3 | AUTO/TRIG → AUTO/FREE → AUTO/TRIG as PWM was removed and restored. |

Builds completed cleanly, and capture-drop/unsafe-capture counts remained zero during the uninterrupted test runs. Validation consists of manual hardware checks; an exhaustive automated state-transition test suite has not been implemented.

### Physical-button checks (2026-11-22 checkpoint)

The following hardware checks passed after clean builds:

| Check | Observed result |
|---|---|
| PE7 and PE8 raw input readings | Released → pressed → released gave 1 → 0 → 1 independently. |
| Run/Stop presses and two-second hold | One action per press; no repeated toggle while held and no action on release. |
| Run/Stop in Single | Arm, cancel a wait, rearm, capture, and automatic Stop behaved as intended. |
| Mode while stopped | NORM → SNGL → AUTO labels changed while STOP and the frozen waveform were preserved. |
| Mode held for two seconds | One mode change; no repeat while held or on release. |
| Physical Single selection | Selecting SNGL did not automatically capture and stop; Run/Stop armed each capture. |
| Mode change while Single waited with PWM low | Returned to AUTO/FREE and continued RUN; restoring PWM returned AUTO/TRIG. |
| Each button held through reset, tested separately | No startup action or release action; the next press performed its normal action. |

Capture-drop and unsafe-capture counts remained zero during uninterrupted Run/Stop testing. Same-pass button-event priority, rapid button chords, minimum press duration, and cancellation at every possible point during rendering have not been separately characterized.

### Shared-control checks (2026-11-23–24 checkpoint)

Hardware testing confirmed the following after clean builds:

| Check | Observed result |
|---|---|
| PE9, PE10, and PE11 raw inputs | Each changed 1 → 0 → 1 independently; existing Run/Stop and Mode controls continued working. |
| Shared debounce refactor | Run/Stop, Mode, hold behavior, independent-button operation, and Single capture retained their previous behavior. |
| SELECT | TIME → VOLTS → LEVEL → EDGE → TIME; one advance per held press; selection remained responsive while stopped with no leftover characters. |
| TIME −/+ | 2/4 ms/div views and labels matched; repeated endpoint presses did not wrap; requests made during Stop applied on resume. |
| VOLTS −/+ | Full-range/1 V/div views, scale labels, and marker positions updated together; stopped views remained held until resume. |
| LEVEL steps and hold | 2048 → 2176 → 2304 and return; one step per press; labels and marker followed the effective level. |
| LEVEL bounds | A decrease from 100 reached 64; an increase from 4000 reached 4031; repeated outward presses stayed at the limits. At 3.25 V the waveform drifted, consistent with the previously observed endpoint limitation. |
| EDGE −/+ | Rising/falling alignment and labels matched; repeated presses preserved the chosen edge; pending changes applied on resume. |
| Single with pending trigger adjustments | Held waveform and labels remained unchanged while stopped; rearming applied the requested falling edge or increased level to the next completed capture. |

The requested Auto-mode adjustment checks confirmed approximately 200 Hz, unchanged input amplitude, and zero capture-drop/unsafe-capture counts during uninterrupted runs. Full-range restoration and trigger-center restoration were included in the tests. Startup-held checks for the original PE7/PE8 buttons are documented above; equivalent startup tests for PE9–PE11 and same-pass adjustment-event conflicts have not been separately validated on hardware.

### Pending-settings checks (2026-11-27 checkpoint)

Hardware testing confirmed the following after a clean build:

| Check | Observed result |
|---|---|
| Auto, initial displayed frame | PEND remained blank when effective requests matched the displayed settings. |
| Stopped TIME adjustment | Requesting 2 ms/div while holding a 4 ms/div frame showed PEND without changing the held scale label. |
| Undo while stopped | Returning the request to 4 ms/div cleared PEND without resuming. |
| Apply on resume | Resuming with a 2 ms/div request updated the waveform and scale label, then cleared PEND. |
| Selection-only change | Changing SELECT while stopped did not show PEND. |
| Normal waiting | A LEVEL increase showed PEND while the old voltage label and marker stayed held; restoring PWM allowed a triggered frame to apply the change and clear PEND. |
| Single capture | An opposite-edge request showed PEND on the stopped capture; rearming applied the new edge and cleared PEND after the capture completed. |
| Equivalent fallback requests | With a held 4 ms/div rising-edge frame, raw requests of 3000 us and edge value 2 left PEND blank because their effective values matched the frame. |

Indicator placement and clean erasure were checked on the display. Vertical-scale fallback equivalence, trigger-limit equivalence, and multiple simultaneous pending settings are supported by the comparison logic but have not been separately hardware-validated for PEND.

These checks preserve normal operation; they do not characterize noise rejection, absolute accuracy, all signal shapes, or all sampling conditions.

## Roadmap

Completed first-version milestones: display Run/Stop, Normal triggering, explicitly armed Single capture, and five physical buttons covering Run/Stop/Single control, mode selection, timebase, vertical scale, trigger level, and trigger edge.

Completed display feedback: a pending-settings indicator with validation across Stop, Normal waiting, Single capture, and equivalent timebase/edge fallback requests.

The next feature remains to be selected from the milestones below.

Later milestones: pre-trigger viewing, rendering performance, calibration and input hardware, and additional measurements such as duty cycle, pulse width, average, and RMS.

Project documentation will include focused architecture, sequence, and state diagrams as useful. These are planned; they are not implemented features.

[Back to projects](../README.md)
