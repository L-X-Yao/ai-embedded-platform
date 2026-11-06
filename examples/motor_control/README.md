# Motor Control

Control a stepper motor via A4988 driver with UART CLI commands and a
table-driven state machine managing motor states (idle/running/homing).

## Wheels Used

| Layer | Wheel | Role |
|-------|-------|------|
| L0 | `cli_parser` | Command parsing from serial input |
| L0 | `state_machine` | Table-driven FSM with entry/exit actions |
| L1 | `a4988` | A4988 stepper driver (Step/Dir/Enable GPIO) |

## What It Demonstrates

1. **GPIO HAL injection** -- The A4988 driver uses a `write_pin` +
   `delay_us` callback pair. Pin assignments are configured via a
   struct, so the same driver works on any MCU.

2. **CLI command interface** -- Four commands parsed from UART input:
   - `run <steps>` -- move N steps clockwise
   - `home` -- run homing sequence (CCW until limit switch)
   - `stop` -- emergency stop from any motion state
   - `status` -- print current state name

3. **State machine pattern** -- Three states (IDLE, RUNNING, HOMING)
   with four events (CMD_RUN, CMD_HOME, CMD_STOP, DONE). Entry actions
   enable/disable the motor and set direction. The transition table is
   static and ROM-able.

4. **Composition** -- CLI events trigger state transitions, state
   entry actions configure the motor driver, and the main loop
   executes steps based on current state. Three independent wheels
   compose without coupling.

## State Diagram

```
         CMD_RUN          DONE/CMD_STOP
  IDLE ---------> RUNNING -----------> IDLE
   |                                    ^
   |  CMD_HOME          DONE/CMD_STOP  |
   +-----------> HOMING  -------------+
```

## Adapting for Your MCU

Replace HAL stubs:

- `hal_gpio_write` -- GPIO pin set/reset for Step/Dir/Enable/MS pins
- `hal_delay_us` -- microsecond delay (DWT or hardware timer)
- `hal_uart_write` -- UART TX for CLI output
- Feed UART RX bytes into `cli_parser_feed(&cli, byte)` from your
  polling loop. A UART ISR has to queue them: feed runs the command handler

## Syntax Check

```bash
gcc -std=c99 -Wall -Wextra -fsyntax-only -I../../tooling/harness main.c
```

Designed for any Cortex-M with GPIO + UART. Tested in host simulation
(syntax-only -- HAL stubs are no-ops).
