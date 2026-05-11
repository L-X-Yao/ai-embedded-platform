// SPDX-License-Identifier: MIT
/*
 * motor_control -- Stepper motor with CLI commands and state machine
 *
 * Demonstrates:
 *   - GPIO HAL injection for motor driver
 *   - CLI command parsing from UART input
 *   - Table-driven state machine for motor states
 *   - Three wheels composing into a control system
 *
 * Wheels used:
 *   L0  cli_parser     -- command parsing from serial input
 *   L0  state_machine  -- table-driven FSM (idle/running/homing)
 *   L1  a4988          -- A4988 stepper motor driver (Step/Dir/Enable)
 *
 * CLI commands:
 *   run <steps>  -- move N steps clockwise
 *   home         -- run homing sequence (move until limit switch)
 *   stop         -- emergency stop
 *   status       -- print current state
 *
 * To adapt: replace GPIO/UART stubs with your MCU peripheral calls.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* --- Wheel headers --- */
#include "../../wheels/drivers/a4988/a4988.h"
#include "../../wheels/primitives/cli_parser/cli_parser.h"
#include "../../wheels/primitives/state_machine/state_machine.h"

/* ===================================================================
 * HAL Implementation (adapt for your MCU)
 * =================================================================== */

static void hal_gpio_write(void *ctx, uint8_t pin_id, bool level)
{
    (void)ctx; (void)pin_id; (void)level;
    /* TODO: HAL_GPIO_WritePin(GPIOx, pin_id, level ? SET : RESET) */
}

static void hal_delay_us(void *ctx, uint32_t us)
{
    (void)ctx; (void)us;
    /* TODO: DWT cycle counter or timer-based microsecond delay */
}

static void hal_uart_write(const char *buf, size_t len, void *ctx)
{
    (void)buf; (void)len; (void)ctx;
    /* TODO: HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 100) */
}

/* ===================================================================
 * HAL binding tables
 * =================================================================== */

static const a4988_hal_t motor_hal = {
    .write_pin = hal_gpio_write,
    .delay_us  = hal_delay_us,
    .ctx       = NULL,
};

/* Pin assignments (adapt for your board) */
static const a4988_cfg_t motor_pins = {
    .step   = 0,  /* PA0 */
    .dir    = 1,  /* PA1 */
    .enable = 2,  /* PA2 */
    .ms1    = 3,  /* PA3 */
    .ms2    = 4,  /* PA4 */
    .ms3    = 5,  /* PA5 */
};

/* ===================================================================
 * State machine: IDLE -> RUNNING -> IDLE, IDLE -> HOMING -> IDLE
 * =================================================================== */

enum motor_state {
    STATE_IDLE    = 0,
    STATE_RUNNING = 1,
    STATE_HOMING  = 2,
};

enum motor_event {
    EVT_CMD_RUN   = 0,
    EVT_CMD_HOME  = 1,
    EVT_CMD_STOP  = 2,
    EVT_DONE      = 3,
};

/* Forward declarations for state actions */
static void on_enter_running(void *ctx);
static void on_enter_homing(void *ctx);
static void on_enter_idle(void *ctx);

static const sm_state_def_t state_defs[] = {
    { STATE_IDLE,    on_enter_idle,    NULL },
    { STATE_RUNNING, on_enter_running, NULL },
    { STATE_HOMING,  on_enter_homing,  NULL },
};

static const sm_transition_t transitions[] = {
    { STATE_IDLE,    EVT_CMD_RUN,  NULL, STATE_RUNNING, NULL },
    { STATE_IDLE,    EVT_CMD_HOME, NULL, STATE_HOMING,  NULL },
    { STATE_RUNNING, EVT_CMD_STOP, NULL, STATE_IDLE,    NULL },
    { STATE_RUNNING, EVT_DONE,     NULL, STATE_IDLE,    NULL },
    { STATE_HOMING,  EVT_CMD_STOP, NULL, STATE_IDLE,    NULL },
    { STATE_HOMING,  EVT_DONE,     NULL, STATE_IDLE,    NULL },
};

/* ===================================================================
 * Motor control context
 * =================================================================== */

static a4988_t         motor_dev;
static state_machine_t motor_sm;
static uint32_t        steps_remaining;

static void on_enter_idle(void *ctx)
{
    (void)ctx;
    a4988_enable(&motor_dev, false);
}

static void on_enter_running(void *ctx)
{
    (void)ctx;
    a4988_enable(&motor_dev, true);
    a4988_set_direction(&motor_dev, A4988_DIR_CW);
}

static void on_enter_homing(void *ctx)
{
    (void)ctx;
    a4988_enable(&motor_dev, true);
    a4988_set_direction(&motor_dev, A4988_DIR_CCW);
    steps_remaining = 10000; /* max steps before timeout */
}

/* ===================================================================
 * CLI command handlers
 * =================================================================== */

static const char *state_name(sm_state_t s)
{
    switch (s) {
    case STATE_IDLE:    return "IDLE";
    case STATE_RUNNING: return "RUNNING";
    case STATE_HOMING:  return "HOMING";
    default:            return "UNKNOWN";
    }
}

static cli_err_t cmd_run(int argc, const char *argv[], void *ctx)
{
    (void)ctx;
    if (argc < 2) return CLI_ERR_ARGS;

    /* Parse step count from argument */
    uint32_t n = 0;
    for (const char *p = argv[1]; *p >= '0' && *p <= '9'; p++) {
        n = n * 10 + (uint32_t)(*p - '0');
    }
    if (n == 0) return CLI_ERR_ARGS;

    steps_remaining = n;
    state_machine_process(&motor_sm, EVT_CMD_RUN);
    return CLI_OK;
}

static cli_err_t cmd_home(int argc, const char *argv[], void *ctx)
{
    (void)argc; (void)argv; (void)ctx;
    state_machine_process(&motor_sm, EVT_CMD_HOME);
    return CLI_OK;
}

static cli_err_t cmd_stop(int argc, const char *argv[], void *ctx)
{
    (void)argc; (void)argv; (void)ctx;
    state_machine_process(&motor_sm, EVT_CMD_STOP);
    return CLI_OK;
}

static cli_err_t cmd_status(int argc, const char *argv[], void *ctx)
{
    (void)argc; (void)argv; (void)ctx;
    sm_state_t s = state_machine_current(&motor_sm);
    const char *name = state_name(s);
    hal_uart_write(name, strlen(name), NULL);
    hal_uart_write("\r\n", 2, NULL);
    return CLI_OK;
}

/* ===================================================================
 * Application
 * =================================================================== */

static const cli_cmd_t commands[] = {
    { "run",    cmd_run,    "run <steps> -- move N steps CW" },
    { "home",   cmd_home,   "home -- run homing sequence"    },
    { "stop",   cmd_stop,   "stop -- emergency stop"         },
    { "status", cmd_status, "status -- print motor state"    },
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static cli_parser_t cli;

int main(void)
{
    /* 1. Initialize motor driver */
    if (a4988_init(&motor_dev, &motor_hal, &motor_pins) != A4988_OK) {
        return 1;
    }
    a4988_set_microstep(&motor_dev, A4988_STEP_QUARTER);

    /* 2. Initialize state machine */
    sm_cfg_t sm_cfg = {
        .transitions      = transitions,
        .num_transitions  = sizeof(transitions) / sizeof(transitions[0]),
        .state_defs       = state_defs,
        .num_state_defs   = sizeof(state_defs) / sizeof(state_defs[0]),
        .user_ctx         = NULL,
    };
    state_machine_init(&motor_sm, &sm_cfg, STATE_IDLE);

    /* 3. Initialize CLI parser */
    cli_cfg_t cli_cfg = {
        .commands     = commands,
        .num_commands = NUM_COMMANDS,
        .write_fn     = hal_uart_write,
        .user_ctx     = NULL,
        .prompt       = "> ",
    };
    cli_parser_init(&cli, &cli_cfg);

    /* 4. Main loop */
    for (;;) {
        /* Feed UART bytes into CLI parser */
        /* TODO: uint8_t byte;
         * if (HAL_UART_Receive(&huart1, &byte, 1, 0) == HAL_OK) {
         *     cli_parser_feed(&cli, byte);
         * } */

        /* Step motor if in motion */
        sm_state_t state = state_machine_current(&motor_sm);
        if ((state == STATE_RUNNING || state == STATE_HOMING) &&
            steps_remaining > 0) {
            a4988_step(&motor_dev);
            steps_remaining--;
            if (steps_remaining == 0) {
                state_machine_process(&motor_sm, EVT_DONE);
            }
        }
    }

    return 0;
}
