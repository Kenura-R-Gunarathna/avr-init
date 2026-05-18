#pragma once

/*
 * btn.h — single button debounce driver
 *
 * Eliminates mechanical bounce by requiring N consecutive identical
 * readings before confirming a state change. Each confirmed transition
 * sets a one-shot edge flag (was_pressed / was_released) that clears
 * automatically after being read.
 *
 * Usage:
 *   1. Define a BTN_Config in main.c with your pin wiring.
 *   2. Call btn_init() once before the scheduler starts.
 *   3. Register task_btn at 20 ms in the scheduler task array.
 *   4. Call btn_was_pressed() / btn_is_held() from other tasks.
 *
 * Wiring: button between the configured pin and GND.
 * The driver enables the internal pull-up — no external resistor needed.
 * Logic is active LOW: pin reads 0 when button is pressed.
 */

#include <stdint.h>
#include <avr/io.h>

/*
 * Pin configuration — fill in main.c, pass pointer to btn_init().
 *
 * @param pin_reg    PIN register for the button pin (e.g. &PINB)
 * @param ddr_reg    DDR register for the button pin (e.g. &DDRB)
 * @param port_reg   PORT register for the button pin (e.g. &PORTB)
 * @param pin_bit    Bit number of the button pin (e.g. PB0)
 * @param threshold  Consecutive same readings required to confirm a state
 *                   change. At 20 ms poll rate: threshold 3 = 60 ms debounce.
 */
typedef struct {
    volatile uint8_t *pin_reg;
    volatile uint8_t *ddr_reg;
    volatile uint8_t *port_reg;
    uint8_t           pin_bit;
    uint8_t           threshold;
} BTN_Config;

/*
 * Initialise the driver. Configures the pin as input and enables the
 * internal pull-up. Call once in main() before scheduler_run().
 *
 * @param cfg  Pointer to a BTN_Config describing your wiring.
 */
void btn_init(const BTN_Config *cfg);

/*
 * Scheduler task — samples the button pin once per call.
 * Register at 20 ms interval. Counts consecutive identical readings;
 * confirms a state change only after reaching cfg->threshold.
 * Always returns 0 (keep 20 ms interval).
 */
uint32_t task_btn(void);

/*
 * Returns 1 if the button is currently in a confirmed pressed state,
 * 0 otherwise. Use for hold detection (e.g. hold to scroll).
 */
uint8_t btn_is_held(void);

/*
 * Returns 1 exactly once after a confirmed press edge, then clears.
 * Use for single-action triggers (increment counter, toggle LED, etc.).
 */
uint8_t btn_was_pressed(void);

/*
 * Returns 1 exactly once after a confirmed release edge, then clears.
 * Use when you need to act on button release rather than press.
 */
uint8_t btn_was_released(void);
