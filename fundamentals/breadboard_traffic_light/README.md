# Breadboard Traffic Light

**Author: Pedro Valle-Cruz**

A register-level STM32F407 traffic-light demonstration using external LEDs, a debounced pedestrian button, and a white crossing indicator. A SysTick-driven state machine keeps the sequence running without blocking delays. The white LED stays steady during the first eight seconds of a requested red phase and blinks during the final two seconds.

Built with CMSIS device headers and `SysTick_Config()`, without HAL drivers. This is a learning demonstration, not a controller for real road traffic.

## Hardware and wiring

- STM32F407 Discovery MB997E / STM32F407VG, powered and programmed through ST-LINK USB
- Breadboard and jumper wires
- Red, yellow, green, and white through-hole LEDs
- Four 330-ohm resistors, one in series with each LED
- Normally open tactile pushbutton
- STM32CubeIDE and supplied CMSIS headers

Disconnect power before changing wiring. Each LED uses this current path:

```text
GPIO output -> 330-ohm resistor -> LED anode (long leg)
LED cathode (short leg) -> common GND
```

| Connection | GPIO | Configuration |
| --- | --- | --- |
| Red LED through resistor | PE7 | Active-high output |
| Yellow LED through resistor | PE8 | Active-high output |
| Green LED through resistor | PE9 | Active-high output |
| Pedestrian white LED through resistor | PE11 | Active-high output |
| Button contact | PE10 | Input with internal pull-up |
| Other button contact and LED cathodes | STM32 GND | Common ground |

The four-leg button has two internally connected contact pairs. Use one contact from each pair, with separate breadboard strips: open when released and connected when pressed. PE10 reads **1 released, 0 pressed**. No external button resistor is required with the configured internal pull-up.

The breadboard supply was used to check the initial LED current paths, but is disconnected for GPIO operation. Do not connect a positive supply rail to the GPIO-driven LED feeds.

The resistor limits each LED's current; it is not a direct GPIO-to-ground load. For illustration, a 3.3 V output and 2.0 V red-LED forward drop would give approximately `(3.3 - 2.0) / 330 = 3.9 mA`. Actual current depends on output voltage and LED characteristics; white LEDs can have less voltage headroom. Brightness was checked visually, not characterized electrically.

Board connection reference: [ST MB997E schematic](https://www.st.com/resource/en/schematic_pack/mb997-f407vgt6-e01_schematic.pdf). Follow the board's printed GPIO labels rather than assuming connector orientation from a photo.

## Behavior

Startup selects red, with the pedestrian indicator off. The repeating sequence is **red -> green -> yellow -> red**.

| Phase | Duration | Pedestrian behavior |
| --- | --- | --- |
| Green, no request | 10 s | White off |
| Green, request accepted | Ends once green has lasted at least 2 s | White remains off; request stays pending |
| Yellow | 5 s | Request retained, white off |
| Red, no request | 10 s | White stays off |
| Red, request pending | 10 s | White steady for 8 s, then toggles every 250 ms until red ends |

An accepted request after the two-second minimum transitions green to yellow on the next state-machine check. A request before that minimum waits until two seconds have elapsed from the start of green, not two seconds from the press.

The request is accepted only on a debounced press during green. Several presses coalesce into one request; holding the button does not repeat it. Presses accepted during yellow or red do not affect the next cycle. A button held at startup is treated as the initial state and requires release and a new press to generate an event.

Red is switched on before the pedestrian LED. At the end of red, white is switched off and the request cleared before green is turned on. The 250 ms toggle interval produces an approximately 500 ms full blink cycle. The first toggle occurs when the red phase reaches eight seconds.

## Build and run

1. Import this directory as an existing STM32CubeIDE managed-build project targeting STM32F407VGTx.
2. Confirm `STM32F407xx` is defined and the CMSIS Core and STM32F4 Device include paths are present in the desired build configuration.
3. Build and flash through ST-LINK. Resume execution if the debugger pauses at `main()`.
4. Observe initial red and the normal sequence.
5. Press the button during green, then observe the yellow interval and requested white indication during red.

The timing assumes the reset clock configuration: **nominal 16 MHz HSI**. `SysTick_Config(16000U)` requests one interrupt per 16,000 processor clock cycles; CMSIS programs a reload of 15,999. Recalculate this argument if the processor clock changes. A nonzero configuration return stops execution in an error loop.

## Implementation

Source: [Src/main.c](Src/main.c).

- `traffic_light_init()` enables GPIOE and sets PE7, PE8, PE9, and PE11 to output mode, deliberately excluding button pin PE10.
- `button_init()` selects input mode and a pull-up for PE10; it relies on the GPIOE clock enabled by the preceding initialization.
- `SysTick_Handler()` only increments the volatile 32-bit millisecond counter. All decisions remain in foreground code.
- The main loop takes a time snapshot, samples and debounces the button, then evaluates a `switch` over an enum representing the light phase.
- Each raw button change restarts a 20 ms stability interval. An accepted stable transition to zero produces a press event.
- Unsigned `now - timestamp` comparisons handle counter wraparound for these short intervals, provided execution checks regularly and does not miss an entire counter period.
- The on/off helpers validate pins 7-9 or 11, then use direct BSRR writes. Toggle reads PE11's ODR output latch and calls the appropriate helper. Main is the sole writer of LED states.

There are no blocking time-delay loops during normal operation. The processor continuously polls; this is nonblocking scheduling, not low-power sleep. The blink timestamp advances to the observed time after each toggle, so prolonged stalls can delay or skip visible blinks rather than replay them.

## Verification

The author performed the following manual hardware tests during development:

- Independent LED output control and off behavior; PE10 read 1 released and 0 pressed
- Increasing SysTick counter and repeating phase transitions
- Early and later presses during green, including the two-second minimum
- Held-button and repeated-press behavior without repeated requests across cycles
- Presses during yellow/red without changing their duration or the following normal green
- Updated normal timing of 10-second green, 5-second yellow, and 10-second red
- White LED off without a request; steady for eight seconds and blinking for two during requested red; off before green
- Holding through another cycle without an unintended white indication

### Counter-wrap test

With the debugger stopped before `SysTick_Config()`, the live `system_ticks` value was set to `0xFFFFF000`. On resume, it wrapped through zero after approximately 4.1 seconds. Initial red still lasted approximately ten seconds before changing to green. The author reported the expected result.

This directly tested the **initial red elapsed-time path across wraparound**. Button debounce and pedestrian blinking use the same subtraction pattern, but were not separately forced across wrap on hardware. Timing observations are approximate, not oscilloscope measurements or automated coverage.

The final cleanup changes comments, header information, formatting, and documentation only; executable source tokens are preserved from the tested version.

## Scope and limitations

- This project is a single traffic-light demonstration with one requested crossing indication, not multiple traffic directions or independent pedestrian scheduling.
- Input is polled and debounced for 20 ms; very short presses may be ignored. A press is classified by the phase when its stable transition is accepted.
- GPIO initialization relies on reset defaults for push-pull type, speed, and initial low output values. It is intended for startup after reset, not arbitrary peripheral reinitialization.
- SysTick is a nominal timebase. Long interrupt masking, debugger halts, or clock changes affect timing.
- The enum has no fault-recovery default branch; a corrupted state is outside the tested scope. There is no watchdog or hardware fault diagnosis.

The agreed functional scope and manual tests are complete, with the wrap-test limits stated above. The next exercise is a **separate HAL version using the same hardware and behavior**, preserving this CMSIS project for comparison.

## What I learned

This project connected external circuits with GPIO registers, enums, nonblocking state machines, debouncing, and wrap-safe elapsed-time comparisons. The pedestrian indicator was added incrementally: steady output first, then a timed blink, with each stage tested before extending it.

Developed through hands-on coding, guided review, and testing on the breadboard.

[Back to Embedded C Fundamentals](../README.md)
