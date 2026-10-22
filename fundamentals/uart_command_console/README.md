# UART Command Console

**Author: Pedro Valle-Cruz**

An embedded C command console for the STM32F407 Discovery board. USART2 receives characters through an interrupt handler and circular buffer; the foreground loop edits and validates complete commands before controlling the four onboard LEDs. The application uses CMSIS register definitions without HAL drivers.

```text
Hello, Pedro!
led 13 on
LED 13 on
led 13 status
LED 13 on
led 13 toggle
LED 13 off
```

## Hardware and connections

- STM32F407 Discovery MB997E, STM32F407VG, powered and programmed through ST-LINK USB
- DSD TECH SH-U09C2 USB-to-TTL adapter with its jumper set to **3.3 V**
- USB-C-to-USB-A adapter for the Mac and jumper wires
- STM32CubeIDE, GNU Arm compiler, supplied CMSIS headers, and macOS Terminal

The purchased adapter has the FT232RNL chip variant. macOS recognized the adapter, and a TXD-to-RXD loopback test passed before connecting it to the board.

Make wiring changes with power disconnected. Keep adapter VCC disconnected; the Discovery has separate USB power.

| Adapter | Discovery | Direction |
| --- | --- | --- |
| RXD | PA2 / USART2_TX / AF7 | STM32 to Mac |
| TXD | PA3 / USART2_RX / AF7 | Mac to STM32 |
| GND | GND | Common reference |
| VCC | Not connected | No adapter power supplied to the board |

RTS and CTS are unused. The external adapter provides serial communication; ST-LINK provides programming/debugging and board power.

| Pin | LED |
| --- | --- |
| PD12 | Green |
| PD13 | Orange |
| PD14 | Red |
| PD15 | Blue |

## Build and run

1. Import this directory as an existing STM32CubeIDE managed-build project targeting STM32F407VGTx.
2. Confirm `STM32F407xx` and the CMSIS include paths are configured: `Drivers/CMSIS/Core/Include` and `Drivers/CMSIS/Device/ST/STM32F4xx/Include`.
3. Build and flash through ST-LINK. Resume execution if the debugger pauses at `main()`.
4. Run `ls /dev/cu.*` in Terminal and identify the adapter's current device name.
5. Open the port at **115200 baud, 8 data bits, no parity, one stop bit, no flow control**. The tested port name was:

   ```sh
   screen /dev/cu.usbserial-BH00M3NI 115200
   ```

6. With the terminal open, press RESET on the board to see the startup greeting. Type `help` and press Enter.

Use the device name actually listed on your Mac. A missing adapter port during testing was resolved by disconnecting and reconnecting its USB connection. Quit `screen` with Control-A, then Control-Backslash, then Y.

The clock setup assumes the reset clock tree: nominal **16 MHz HSI**, undivided APB1, and 16x USART oversampling. BRR is `0x008B`, the rounded divider for a 115200-baud target. USART format settings rely on their 8N1 reset defaults. Recalculate the divider if the peripheral clock changes.

## Commands and editing

```text
help
led <12-15> <on|off|status|toggle>
```

| Example | Behavior |
| --- | --- |
| `led 12 on` | Turn green on |
| `led 13 off` | Turn orange off |
| `led 14 status` | Report the red output state without changing it |
| `led 15 toggle` | Reverse blue's output state and report the result |

Commands are case-sensitive. `help` must match exactly. LED commands are tokenized on spaces; repeated spaces are skipped. The old green-only forms such as `led on` have been replaced by numbered commands.

- Enter (`CR` or `LF`) ends a line; a CRLF pair leaves an ignored empty second line.
- Backspace or DEL removes one stored character and erases its echo.
- Empty lines are ignored.
- Missing or extra tokens produce usage text. Invalid pins produce a pin error; unsupported actions produce `Unknown command`.
- Decimal conversion must consume the entire pin token and produce 12–15. `13abc` is rejected; numeric forms such as `+13` and `013` are accepted by `strtol()`.
- Status reads the GPIO output latch, not an optical measurement of the LED.

## Implementation

Application source: [Src/main.c](Src/main.c).

### Two different buffers

| Buffer | Purpose | Capacity |
| --- | --- | --- |
| Receive ring, `rx_buffer` | Temporarily hold bytes delivered by the ISR | 64 slots, 63 usable; one remains unused to distinguish full from empty |
| Command line, `line` | Accumulate text for parsing and editing | 63 characters plus a null terminator |

The ISR owns `rx_head`; foreground code consumes through `rx_tail`. The producer stores a byte before publishing the new head. The reader copies the byte before releasing its slot. Both indexes wrap modulo `RX_BUFFER_SIZE`.

`main()` briefly masks interrupts while checking error-counter changes, flushing if necessary, and attempting one dequeue. It restores the saved PRIMASK state before printing or parsing. Shared variables are volatile, but the protected section supplies coordination; volatile alone is not a lock.

### Receive, transmit, and commands

- `USART2_IRQHandler()` snapshots SR, reads DR when receive/error flags are present, counts errors, and either discards the byte or queues it. It performs no string parsing or terminal output.
- `usart2_try_read_char()` returns immediately with success/failure. The foreground loop keeps checking when the queue is empty.
- Transmission still polls TXE and blocks until DR can accept a byte. TXE does not mean the last transmitted bit has left the pin.
- `process_command()` modifies the completed line with `strtok()`, rejects missing/extra arguments, validates the command name and pin, and dispatches the requested action.
- GPIO helpers restrict access to PD12–PD15. Direct BSRR writes set/reset pins; ODR supplies status. Toggle assumes no other execution context changes those LED outputs.

### Error recovery

Four ISR-owned counters track distinct problems:

| Counter | Meaning |
| --- | --- |
| `rx_dropped_count` | New byte discarded because the software receive queue was full |
| `rx_overrun_count` | USART hardware overrun (ORE) |
| `rx_framing_count` | Framing error (FE) |
| `rx_noise_count` | Noise flag (NF in the manual, `USART_SR_NE` in the supplied header) |

The handler reads SR before DR and counts each flagged error separately, allowing multiple counters to increment for one event. Error-marked data is not queued.

When a counter changes, foreground code clears the partial command and flushes queued bytes, including any old delimiters. It discards subsequent input until a newly received CR/LF, then prints `Error: receive data lost; retype command`. If the original Enter was flushed or lost, press Enter again, then retype the command. Counters remain available for debugging rather than being cleared by foreground code.

A command-line overflow is separate: once the line exceeds 63 characters, it remains invalid even if Backspace later frees space. Enter reports `Error: line too long` and resets the line state.

## Verification

The author reported successful builds and hardware regression checks on the final functional implementation:

- Adapter loopback, startup greeting, character echo, and normal interrupt-driven reception
- On, off, status, and toggle for each LED pin 12–15, with other LEDs unchanged
- Help, empty lines, Backspace editing, and Backspace on an empty line
- Rejection of invalid numeric text, out-of-range pins, unsupported actions, missing arguments, and extra arguments
- Overlong-line rejection followed by a valid help command

Additional controlled tests:

| Test | Method and observed result |
| --- | --- |
| Hardware overrun | Paused the CPU, sent characters, resumed, then pressed Enter. Recovery message appeared; the next help command worked. |
| Software ring overflow | Temporarily used eight slots and held foreground consumption while interrupts ran. Eleven typed bytes produced four software drops and zero hardware overruns. Releasing the hold and sending Enter produced recovery; temporary test code was removed and the ring restored to 64 slots. |
| Framing recovery path | Increased the live framing counter in the debugger during partial input. The line was rejected and subsequent help worked. |
| Noise recovery path | Repeated the counter-change test for noise, with successful recovery. |

**The framing/noise tests exercised foreground recovery only.** They did not generate electrical faults or prove hardware FE/NF detection and ISR counting. No physical framing/noise fault test was performed. Comments and documentation were added afterward without changing executable source tokens.

## Limitations

- TX polling has no timeout. Sustained input can exceed the finite queue while the foreground loop prints responses; this is not a lossless streaming protocol.
- The foreground loop busy-polls the queue rather than sleeping when idle.
- Error handling covers ORE, FE, and NE observed by the handler; parity checking is disabled in the current 8N1 setup.
- Editing supports Backspace/DEL only. Tabs, arrow-key escape sequences, and other control characters are not filtered; only spaces are token delimiters.
- Recovery intentionally discards queued input, potentially including complete later commands, to restore a known line boundary.
- Counter wraparound is not specially tracked. Peripheral initialization assumes a reset baseline for settings not explicitly written.

These limitations define the current learning-project scope. Optional extensions include bounded or buffered transmission, richer input filtering, and physical error-injection tests. The separate breadboard traffic-light project is the planned next hardware exercise.

## What I learned

This project grew from transmitting one character to parsing validated commands for four LEDs. It connected register masks, alternate functions, baud-rate calculation, C strings and pointers, numeric conversion, ring-buffer ownership, short critical sections, and interrupt-driven recovery. The controlled tests also made the difference between a hardware overrun and a full software queue visible.

Developed incrementally through hands-on implementation, guided review, and testing on the board.

[Back to Embedded C Fundamentals](../README.md)
