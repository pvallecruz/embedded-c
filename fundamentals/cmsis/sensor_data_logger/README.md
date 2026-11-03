# Sensor Data Logger — CMSIS

**Author: Pedro Valle-Cruz**

A button-controlled light-sensor logger on the STM32F407G-DISC1 (MB997E). Firmware written in C with CMSIS register access samples ADC1 channel 1 and transmits timestamped readings through USART2. A Python receiver validates the incoming fields and saves a new CSV for each run.

Built incrementally with guided review; the author wrote the application code and manually tested the hardware. No HAL APIs are used in the firmware.

## Behavior

- Starts paused, with the green LED off.
- One accepted button press starts sampling and turns the LED on. A second press pauses sampling and transmission and turns the LED off.
- Holding the button does not repeatedly toggle logging. The reused button module requires 20 ms of stable input and does not count a button held at initialization as a new press.
- While enabled, sampling becomes due every 100 ms. Starting logging resets the scheduling timestamp, so the first sample is due 100 ms later.
- SysTick continues while paused. Resumed output therefore contains a timestamp gap.
- A conversion timeout increments `adc_timeout_count`, turns the LED off, pauses logging, and resets/reinitializes the ADC block. No measurement is transmitted for the failed conversion. The next button press requests a new attempt.
- ADC readings are raw 12-bit values, 0–4095, not calibrated lux measurements.

## Wiring

Disconnect USB power before changing wiring. Power the Discovery board through ST-LINK USB; leave the breadboard power-supply module disconnected. All grounds are common.

| Connection | Wiring |
| --- | --- |
| Light divider | Board 3V → photoresistor → junction → 10 kΩ resistor → GND |
| ADC input | Divider junction → PA1 |
| Logging button | PE10 → normally open switch → GND; internal pull-up enabled |
| Green status LED | PE9 → 330 Ω resistor → LED anode; cathode → GND |
| Serial data | PA2 (USART2 TX) → adapter RXD |
| Serial ground | Board GND → adapter GND |

The adapter used is a DSD TECH SH-U09C2, identified by its listing as FT232RL-based. Set its logic selection to 3.3 V. Leave adapter VCC, TXD, RTS, and CTS disconnected for this transmit-only setup. For a four-leg button, select contacts that have continuity only when pressed.

More light reduces the photoresistor resistance and raises the divider-junction voltage. Keep the ADC input within the board's analog supply range; do not feed it 5 V.

## Firmware

Import this directory as an existing STM32CubeIDE project and build Debug. Select the `sensor_data_logger` executable when launching the debugger.

Both Debug and Release are configured with `STM32F407xx` and these C include directories:

```text
../Drivers/CMSIS/Core/Include
../Drivers/CMSIS/Device/ST/STM32F4xx/Include
```

| File | Responsibility |
| --- | --- |
| `Src/main.c` | ADC, SysTick, USART2, sample scheduling, and logging state |
| `Src/button.c`, `Inc/button.h` | Reused PE10 input and debounced press-event module |
| `Src/led.c`, `Inc/led.h` | Reused validated GPIOE LED control |
| `logger.py` | Host serial reception, validation, and CSV capture |

Clock assumptions were checked in the debugger: HSI enabled/ready, RCC CFGR zero, and ADC common CCR zero. The nominal system/APB clocks are 16 MHz; ADC uses APB2 divided by two, or 8 MHz. SysTick uses 16,000 processor cycles per millisecond. Clock changes require revisiting SysTick and USART baud configuration.

ADC1 uses PA1 in analog mode with no pulls, 12-bit resolution, one regular conversion of channel 1, 480-cycle sampling, right alignment, and software triggering. After enabling ADC1, initialization waits for two SysTick increments. This is a blocking startup wait; ordinary sampling is scheduled using unsigned elapsed-time subtraction.

USART2 uses PA2 AF7 and a BRR value of 139 for nominal 115200 baud at 16 MHz, with reset-default 8N1 and 16× oversampling. Transmission and ADC completion polling are blocking; the interval between samples is not a blocking delay. ADC completion polling is now bounded by five elapsed SysTick ticks, provided interrupts keep running. UART TXE polling and the final TC wait are each bounded by five elapsed ticks, with failures propagated to the main loop.

## Run the Python receiver

Python 3 and the `pyserial` package are required. The module is imported as `serial`; `csv` and `datetime` are included with Python. One isolated setup option, from the project directory:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install pyserial
python logger.py
```

Alternatively, use an existing Python environment with pySerial installed. My working setup used Homebrew Python with pySerial. Keep local environments out of version control.

Update `PORT` in `logger.py` for your adapter. The tested Mac device was `/dev/cu.usbserial-BH00M3NI`; names can differ between machines/adapters. Close `screen` or any other program using that port before starting Python.

1. Connect the board and serial adapter and run the firmware.
2. Run the receiver from the directory where you want captures saved.
3. Press the physical button to begin readings; press again to pause.
4. Press Control+C in Terminal to stop the receiver cleanly.

The receiver creates `photoresistor_YYYYMMDD_HHMMSS_microseconds.csv` in the current working directory using exclusive creation (`x` mode). Existing files are not overwritten. The file remains open during button pauses; all active periods in one receiver run belong to the same capture.

## Data format

Firmware transmits ASCII rows with a CRLF ending and no header:

```text
5100,1482
5200,1485
```

The first field is the firmware millisecond counter sampled before conversion, not a wall-clock timestamp. The second is the raw ADC result. The Python receiver adds:

```text
Time (ms),ADC_Reading
```

The receiver bounds each read to 64 bytes and retains partial records across timeouts until a newline arrives. On connection it discards through the first newline to establish a known boundary, deliberately losing one potentially complete sample. Records longer than 64 bytes are dropped; their remainder is discarded through the next newline before normal reception resumes. An unfinished record is not written on shutdown.

It skips empty timeout reads, non-ASCII input, rows with other than two fields, failed integer conversions, timestamps outside 0–4294967295, and ADC values outside 0–4095. Accepted rows are written and flushed, and their values are printed. Flushing moves Python's buffered output to the operating system; it is not a guarantee against sudden power loss.

For an Excel plot, use XY Scatter with straight lines: timestamps as X and ADC readings as Y, excluding the header. Keep captures separate. A line across a pause connects measurements on either side; it does not represent measurements taken during the pause. CSV preserves values, while an XLSX workbook preserves the chart.

## Verification

I manually confirmed the following during October 30–November 1, 2026:

| Test | Observed result |
| --- | --- |
| Potentiometer bring-up | Endpoint/midpoint readings of 0, 2039, and 4095 |
| Timer startup wait and refactor | ADC readings continued working |
| Sample scheduling | Elapsed value was 100 at successive sampling breakpoints; transmitted timestamps advanced by 100 ms during normal runs |
| Photoresistor | Approximately 400–500 covered, 1383–1386 in room light, and 3950 under a flashlight in one test; later plotted captures showed similar separation |
| Button and status LED | Paused startup, start/stop, held-button behavior, and resume with a timestamp gap worked |
| Python capture | Timestamped CSV contained a header and numeric measurements |
| Control+C | Friendly shutdown without a traceback |
| Adapter absent | Serial error and return to prompt; no new capture created |
| Adapter disconnected during capture | Serial error and return to prompt; earlier captured rows remained in the CSV |
| Existing CSV | Existing-file message without overwriting the capture |
| Invalid output path | File/OS error message and return to prompt |

These are manual functional checks, not instrumented timing/accuracy measurements. Debugger pauses affect execution timing. Light readings depend on illumination and component characteristics. Release was configured but not independently validated in this session. Receiver validation and framing were also exercised using simulated serial input, as detailed below; these checks do not simulate electrical UART faults.

### Receiver framing verification and final review

On November 1, 2026, the saved Python receiver passed five simulated serial-input scenarios: fragments separated by an empty timeout; oversized records with a later newline; oversized records ending in the current read; invalid ASCII/field counts/integers/ranges plus valid boundary values; and startup discard plus an unfinished record at shutdown. Tests compared the resulting CSV rows against expected values and checked clean resource closure. No real serial port was opened for these simulations.

After those changes, I repeated normal capture on hardware and confirmed clean output. Final formatting and explanatory-comment cleanup preserved the C executable tokens and Python syntax tree. The reusable button and LED modules were left unchanged. The cleaned Debug firmware rebuilt and linked with no warnings or errors reported; Python parsed successfully and the Git whitespace check passed. The board was not reflashed after this comments/formatting-only cleanup, and Release was not rebuilt.

### ADC timeout and recovery milestone

On November 1, 2026, I verified normal readings after changing `adc_read()` to return an integer status and write the measurement through a `uint16_t *` only on success. Failure leaves the supplied output unchanged. The main loop transmits only successful measurements and retains the previous `adc_reading` for debugger inspection.

The timeout starts before SWSTART and returns failure after at least five elapsed tick counts while EOC remains clear. This is a millisecond-counter threshold, not an instrumented guarantee of exactly 5 ms; tick phase and interrupt latency affect wall-clock timing. The timeout and ADC startup wait both depend on working SysTick interrupts.

On failure, the caller increments `adc_timeout_count`, pauses logging, turns off PE9, pulses `RCC_APB2RSTR_ADCRST`, and calls `adc_init()`. This reset affects the shared ADC block, including ADC2 and ADC3; this application uses only ADC1. Reinitialization is a recovery attempt, not proof the underlying fault is gone. A persistent fault will time out again on the next requested sample.

Manual fault tests confirmed:

- **Forced non-completion:** break at SWSTART, execute that statement, disable ADC1 by setting CR2 to zero, clear SR, then resume. The counter increased once, logging and the LED stopped, and the previous reading remained. After ADC recovery, a button press restored readings without a board restart.
- **Tick wraparound:** repeat with both `system_ticks` and the local `started_at` set to `0xFFFFFFFD`. At the timeout return, unsigned elapsed subtraction reached the five-tick threshold across zero. Pausing and button-driven recovery again behaved as expected.

Breakpoints must allow execution to resume so SysTick advances. Restart normally after debugger-injected clock values. These tests cover controlled ADC non-completion and wrap arithmetic, not all possible clock, bus, or electrical faults. They do not establish precise analog timing.

### UART timeout and partial-line recovery milestone

On November 2, 2026, normal UART capture and button start/pause/resume were confirmed with `uart_timeout_count` remaining zero. Each character waits for TXE with a five-tick timeout; the string function stops at the first failure and separately waits for TC with the same timeout. This is a per-wait bound, not a single five-tick budget for the entire string. Both depend on SysTick interrupts. Success means local transmission completed, not that Python received the bytes.

A failed data line increments `uart_timeout_count`, pauses logging, turns the LED off, sets `uart_resync_needed`, and resets/reinitializes USART2. Before a later button-driven restart can send data, it must successfully transmit `!\r\n`. The exclamation mark makes any buffered numeric prefix invalid, and the newline establishes a fresh boundary. Python rejects it through its existing field/integer validation. A failed marker increments the counter again, resets USART2, and leaves the flag set and logging paused for another button retry.

Manually confirmed checks:

- **TC timeout:** break after the character loop and before capturing the TC wait start time; disable USART2 with CR1 = 0 and clear SR, then resume. One timeout was counted, output paused, the LED turned off, and a button-driven marker/restart restored readings. The final data line may already have reached the host before the injected fault.
- **Partial line:** a temporary `uart_test_stall` flag was added to the TXE wait condition. After several good records, a breakpoint conditioned on `character == 13` injected the stall before the carriage return. Python retained the numeric prefix across timeouts without writing it. Clearing the stall and restarting by button sent the marker, discarded that prefix, and restored clean capture in the same Python session.
- **Failed marker:** leaving the test flag set during the next button press caused another timeout. Logging remained paused, the LED stayed off, and `uart_resync_needed` remained set. Clearing the flag allowed a later press to recover normally.

The temporary flag and modified wait condition were removed after testing. The character-stall test exercises the software timeout path, not a measured physical TXE hardware failure. UART timeout wraparound was not separately injected; only the ADC wraparound test above was performed. Disconnecting the adapter does not establish a firmware UART stall because the transmitter does not require a receiver acknowledgment.

## Current limitations and follow-up

- ADC and UART waits have timeouts, but those timeouts and ADC recovery startup depend on SysTick. Stopped interrupts or a processor/bus fault are outside this recovery mechanism. Polling is still blocking during each bounded wait.
- Newline framing and range checks do not prove loss-free delivery or detect every corrupted numeric value. There are no sequence numbers, checksums, or acknowledgments. If firmware resets during a partial record, the receiver cannot reliably identify the new session from this protocol alone.
- Serial/file errors stop the receiver; automatic reconnection is not implemented. Error handlers print a message but do not explicitly set a failure exit code.
- The millisecond counter wraps after approximately 49.7 days and restarts on reset. Firmware elapsed-time comparisons use unsigned subtraction; CSV timestamps are not unwrapped by Python.
- The receiver has no live graph, calibration, or LCD support yet.

Keep generated Debug/Release outputs and local environments out of commits. Include capture data only when intentionally selecting an example for documentation. No code changes are required to switch from the tested potentiometer input to the described photoresistor divider.

## Completed scope

The light-sensor logger milestone is complete: button-controlled 100 ms sampling, LED state feedback, timestamped UART transmission, validated CSV capture, and documented hardware/error-path checks. The ADC timeout/recovery extension is also hardware-tested, including tick wraparound. UART timeout, partial-line resynchronization, and failed-marker retry behavior are also tested within the coverage limits above. Live visualization, LCD output, and calibration remain separate extensions.

[CMSIS project index](../README.md) · [Fundamentals index](../../README.md)
