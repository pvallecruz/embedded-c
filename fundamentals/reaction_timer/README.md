# Reaction Timer

A button-and-LED reaction game built with register-level C on the STM32F407 Discovery board. Start a round, wait for the green LED, and press the USER button as quickly as possible. The game reports the reaction time and the fastest valid time so far through the SWV console.

[Read the application code](Src/main.c) · [Back to Embedded C Fundamentals](../README.md)

## How to play

1. When the console prompts you, press and release the USER button.
2. Wait for the green LED. Each round selects a waiting interval between 2,000 and 5,000 ms.
3. Press the button when the LED turns on.
4. Read the reaction time and best time in the console.
5. Release the button to return to READY and play another round.

A press before the LED signal cancels the round. After release, the console prints `Too soon!` and prompts for another round. Holding a button does not start additional rounds or produce repeated results.

## Hardware and tools

- STM32F407 Discovery board
- USB data cable connected to the onboard ST-LINK port
- STM32CubeIDE with SWV ITM Data Console

No external components or jumper wires are needed.

| Component | Pin | Behavior |
| --- | --- | --- |
| USER button (B1) | PA0 | HIGH when pressed |
| Green LED | PD12 | Turns on to signal the player to react |

Use the USER button, not RESET.

## What I implemented

- **Direct register access:** configure GPIO clocks and modes, read IDR, and control the LED with BSRR. The application uses no HAL calls; the project retains CubeIDE startup and runtime support.
- **SysTick timekeeping:** increment a shared `volatile` counter through the timer interrupt.
- **Explicit game states:** separate starting, release handling, waiting, measuring, valid results, and rejected rounds.
- **Varying delays:** seed C's pseudorandom generator once using the tick count at the first accepted start-button release; choose one duration per round with `2000U + ((uint32_t)rand() % 3001U)`.
- **Debounced input:** accept a raw state change after it remains unchanged for 20 ticks.
- **Reaction timestamps:** record the beginning of the final stable HIGH interval when a press is accepted, then use that timestamp for elapsed time rather than the later debounce confirmation time.
- **Early-press validation:** reject presses accepted during WAITING, and also reject a press whose recorded timestamp precedes the LED signal if confirmation arrives during MEASURING.
- **Best-time tracking:** update the record only for a faster valid result. The first valid round establishes the record.
- **Console feedback:** show the READY prompt once per entry, announce new records, print valid results, and report rejected rounds.

## Game states

| State | Responsibility | Next state |
| --- | --- | --- |
| `GAME_READY` | Prompt once and wait for a stable press | `GAME_WAIT_RELEASE` |
| `GAME_WAIT_RELEASE` | Wait for stable release, choose the delay, and record its start | `GAME_WAITING` |
| `GAME_WAITING` | Detect an early press or wait for the selected interval to expire | `GAME_TOO_SOON` or `GAME_MEASURING` |
| `GAME_MEASURING` | Validate the accepted press timestamp and calculate a valid result | `GAME_TOO_SOON` or `GAME_RESULT` |
| `GAME_RESULT` | Wait for stable release after reporting a result | `GAME_READY` |
| `GAME_TOO_SOON` | Wait for stable release, then report rejection | `GAME_READY` |

## Timing and debounce

SysTick uses the processor clock with a reload value of `15999`. At **16 MHz**, this gives a nominal **1 ms tick**. A change must persist for **20 ticks** before becoming the stable input state. Update the reload value and SWV core-clock setting if the processor clock changes.

The debounce filter and game waiting period use elapsed-time checks rather than blocking delay loops. Reaction time is calculated as:

```c
reaction_time_ms = accepted_press_tick - reaction_start_tick;
```

`accepted_press_tick` records the last raw transition leading into the confirmed HIGH interval. This avoids adding the confirmation interval to the result, but remains a sampled, millisecond-resolution measurement rather than an exact measurement of physical contact or human response.

## Build and run

1. Clone or download the repository and import `fundamentals/reaction_timer` as an existing project in STM32CubeIDE.
2. Connect the board through ST-LINK and build the project.
3. In the project's Debug Configuration, enable SWV and set the core clock to **16 MHz**.
4. Start debugging and remain paused at `main()`.
5. Open **SWV ITM Data Console**, enable **ITM stimulus port 0** in Configure Trace, and click **Start Trace**.
6. Resume execution and follow the console prompt.

`printf` is redirected through `_write()` to ITM port 0. The current character-output implementation waits for ITM readiness, so configure tracing before running the game; operation without trace setup is not guaranteed.

To inspect local variables in Expressions, suspend execution and select the `main()` stack frame. A breakpoint in the valid-result branch is useful for inspecting `reaction_time_ms`, `best_time_ms`, and `wait_duration_ms`. Resume for gameplay; pauses interrupt the timing experiment.

## Hardware verification

Manual checks were performed during development on **October 5–6, 2026** and reported passing by the developer:

| Check | Observed result |
| --- | --- |
| Inspect SysTick counter between run/suspend observations | Counter increased |
| Hold start button, then release | Waiting began after release |
| Complete a valid round | Reaction time was stored and reported through SWV |
| Repeat rounds | Game returned to READY and could be played again |
| Inspect selected delays | Observed 2,828 ms and 3,508 ms on successive rounds |
| Press before the LED | Rejected round reported `Too soon!` |
| Hold a response button | Only one result; READY prompt waited for release |
| Return from valid and rejected rounds | READY prompt appeared once per return |
| Exercise best-time behavior | Best-time reporting behaved as intended |
| Test the final debounce and timestamp changes | Valid, early-press, and held-button behavior reported working as intended |

These are manual functional checks, not exhaustive automated tests. The exact tick period and presses near the LED transition have not been independently measured with external equipment. The two observed delay values demonstrate variation, not the distribution of all possible values.

## Limitations

- The best time is stored in RAM and resets when the board restarts.
- The delay generator is pseudorandom; it is not a source of true randomness.
- Button input is sampled by the main loop. Bounce, sampling intervals, and tick resolution affect recorded timing.
- The game waits indefinitely for a response after the LED turns on; there is no response timeout yet.
- SWV output depends on an active, correctly configured debug connection.

## What I learned

- A state assignment changes which case runs on the next loop iteration; it does not jump out of the current block.
- `if/else` makes an early rejection and a valid result mutually exclusive.
- A release state prevents a held button from becoming the next action.
- Seeding happens once, while choosing a duration happens each round.
- Remembering whether a prompt has been shown prevents repeated output inside the main loop.
- Debounce confirmation time and the recorded press time serve different purposes.
- Debugger expressions for local variables depend on the selected stack frame.

## Next milestones

- [x] Initialize GPIO and SysTick
- [x] Implement the game state machine and replay
- [x] Vary the waiting interval between two and five seconds
- [x] Report valid and early presses through SWV
- [x] Track the best valid reaction time
- [x] Debounce input and validate accepted press timestamps
- [ ] Add a short demonstration video
- [ ] Measure timing and boundary behavior with external equipment
- [ ] Consider a response timeout and a way to clear the best time
