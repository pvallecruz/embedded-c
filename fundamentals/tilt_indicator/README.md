# SPI Tilt Indicator

A register-level C project that reads the onboard LIS3DSH accelerometer and uses two LEDs to indicate left or right tilt. Hysteresis keeps the indication stable near the switching threshold, while SPI timeouts let the application report a communication failure and stop processing measurements.

[Read the application code](Src/main.c) · [Back to Embedded C Fundamentals](../README.md)

## Behavior

- Tilt left in the tested board orientation: the green LED turns on.
- Tilt right: the red LED turns on.
- Return near level: both LEDs turn off.
- A sensor identity mismatch or communication error stops the application until reset.

Directions follow the sensor X-axis sign and depend on how the board is held. This is a raw-acceleration threshold demonstration, not a calibrated angle meter.

## Hardware and tools

- STM32F407 Discovery board with the onboard **LIS3DSH** accelerometer (the board used for this project is revision E)
- USB data cable connected to the onboard ST-LINK connector
- STM32CubeIDE and SWV ITM Data Console for diagnostic messages

No external components or jumper wires are required. Check the sensor fitted to your board: other board revisions may use a different accelerometer.

| Connection | MCU pin | Purpose |
| --- | --- | --- |
| SPI1 SCK | PA5, AF5 | Serial clock |
| SPI1 MISO | PA6, AF5 | Data from sensor |
| SPI1 MOSI | PA7, AF5 | Data to sensor |
| Sensor CS | PE3 | Active-low chip select |
| Green LED | PD12 | Left indication |
| Red LED | PD14 | Right indication |

## What I implemented

- GPIO clocks, modes, and alternate functions through direct register access.
- SPI1 master operation: full duplex, 8-bit frames, MSB first, mode 3, and a divide-by-16 prescaler. With the assumed 16 MHz peripheral clock, SPI runs at 1 MHz.
- Single-register reads and writes with explicit chip-select control. A dummy byte generates the clock pulses needed to receive a register value.
- Startup validation of `WHO_AM_I` at `0x0F` against the expected `0x3F`.
- Configuration of `CTRL_REG4` at `0x20` with `0x5F`: 50 Hz output, block data update, and X/Y/Z enabled.
- Reading X low/high registers (`0x28` and `0x29`) and combining them into an `int16_t` measurement.
- A nominal 1 ms SysTick counter and a 100 ms application sampling interval.
- Three tilt states with separate entry and release thresholds.
- BSRR writes that set one LED and reset the other in a single operation.
- Separate status returns and output pointers, so a received byte cannot be confused with an error code.
- Timeouts for TXE, RXNE, and BSY waits, with chip-select release on transaction completion or failure.

The application uses no HAL calls; CubeIDE startup and runtime support remain part of the project.

## Hysteresis

Thresholds are signed raw counts, not degrees. Opposite-direction entry is checked before returning to level, allowing direct left-to-right transitions and vice versa.

| Current state | Reading | Next state |
| --- | --- | --- |
| Level | X < -2000 | Left |
| Level | X > 2000 | Right |
| Level | Otherwise | Level |
| Left | X > 2000 | Right |
| Left | Otherwise, X >= -1500 | Level |
| Left | Otherwise | Left |
| Right | X < -2000 | Left |
| Right | Otherwise, X <= 1500 | Level |
| Right | Otherwise | Right |

For example, at X = 1800, a previous RIGHT state stays RIGHT, while a previous LEVEL state stays LEVEL. This avoids repeatedly switching near the entry threshold.

## Timing and error handling

SysTick reload is `15999`, using the processor clock. At 16 MHz this provides a nominal 1 ms tick. Sampling uses unsigned elapsed-time subtraction; it does not block for a 100 ms delay.

`SPI_TIMEOUT_MS` is 10 ticks **per waiting phase**, not a 10 ms budget for an entire transaction. TXE and RXNE each have their own starting timestamp, and deselection has a separate BSY wait. Cleanup can therefore add another wait after a transfer timeout. SysTick must remain enabled and its interrupt must run for these checks to advance.

| Failure | Diagnostic | Application response |
| --- | --- | --- |
| Startup ID transfer times out | `Communication error` | Stop before sensor configuration |
| ID differs from `0x3F` | `Unexpected ID` | Stop before sensor configuration |
| Configuration write times out | `Configuration error` | Stop before sampling |
| X measurement transfer times out | `Timeout error` | Turn both LEDs off and stop |

The program does not retry or recover the SPI peripheral automatically. Reset restarts initialization. Output values must only be used after `SPI_OK` is returned.

## Build and run

1. Import this directory as an existing project in STM32CubeIDE and build it.
2. Connect the board through ST-LINK and launch debugging.
3. Enable SWV in the debug configuration with a core clock of **16 MHz**.
4. While paused at `main()`, configure the SWV ITM Data Console for **stimulus port 0** and start tracing.
5. Resume and slowly tilt the board left and right, then return it flat.

Normal operation uses LED feedback; the current application prints messages only for errors. The ITM character-output routine waits for readiness, so diagnostic output depends on trace configuration and is not covered by the SPI timeouts. If changing the clock setup, update SysTick reload and the SWV clock setting too.

## Verification

The following manual checks were reported passing during development on October 8, 2026:

| Check | Reported result |
| --- | --- |
| Read sensor identity | `0x3F` received |
| Read back configuration during development | `0x5F` received |
| Observe X values | Approximately 39–120 flat, -9100 left, and +8300 right in the tested positions |
| Sample every 100 ms | Continuous output slowed as expected during development |
| Left, right, and level feedback | Matching LED or both off; direct direction changes worked |
| Hysteresis | LED remained on until the release threshold region was reached |
| Temporarily change expected identity to `0x00` | Mismatch reported; no tilt response |
| Inject `SPI_TIMEOUT` after a sampling call | `Timeout error`; tilt response stopped |
| Force TXE waiting condition true, retaining its timeout check | Startup communication error; no tilt response |
| Force RXNE waiting condition true, retaining its timeout check | Startup communication error; no tilt response |
| Force BSY waiting condition true, retaining its timeout check | Startup communication error; no tilt response |
| Restore original conditions and remove injected status | Normal operation restored |

These are manual functional tests. Forced-loop tests simulate waits that do not complete; they are not physical bus-fault tests. The exact timeout duration, clock frequency, and chip-select waveform were not measured with external equipment. The configuration-error branch has not been separately fault-injected. The readback and raw-value prints used during development are not part of the current normal output.

## What I learned

- A peripheral register address and the value written to it serve different purposes.
- SPI receives a byte whenever it transmits one, even when that received byte is discarded.
- Pointer parameters let a function return status separately from its data.
- Successful transfers must be checked before interpreting received measurements.
- State memory and separate thresholds prevent boundary flicker.
- Error handling includes releasing chip select and propagating failure through callers.
- Testing normal behavior alone does not exercise failure paths.

## Limitations and next milestones

- Only X is used; motion and gravity both affect the measurement.
- Thresholds are selected from observed raw values, without angle conversion or calibration.
- Polling SPI waits block execution until completion or timeout; there is no DMA or interrupt-driven SPI.
- No automatic retry, watchdog recovery, or dedicated error LED is implemented.
- Add a short demonstration video showing direction changes and hysteresis.
- Consider external timing measurements and a dedicated configuration-failure test.
