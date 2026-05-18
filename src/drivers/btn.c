#include "btn.h"

static const BTN_Config *cfg;

static uint8_t confirmed;      /* last confirmed state: 0=pressed, 1=released */
static uint8_t count;          /* consecutive same-reading counter             */
static uint8_t flag_pressed;   /* one-shot edge flag: set on confirmed press   */
static uint8_t flag_released;  /* one-shot edge flag: set on confirmed release */

/*
 * Initialises the driver.
 * Configures pin as input and enables the internal pull-up so the pin
 * sits at VCC (reads 1) when the button is open. Pressing the button
 * connects the pin to GND (reads 0) — active LOW logic.
 *
 * @param c  Pointer to a BTN_Config with wiring details.
 */
void btn_init(const BTN_Config *c) {
    cfg       = c;
    confirmed = 1;   /* assume released at startup (pull-up = HIGH = 1) */
    count     = 0;
    flag_pressed  = 0;
    flag_released = 0;

    *cfg->ddr_reg  &= ~(1 << cfg->pin_bit);  /* pin as input               */
    *cfg->port_reg |=  (1 << cfg->pin_bit);  /* enable internal pull-up    */
}

/*
 * Samples the button pin and runs the debounce counter.
 *
 * Each call reads the current pin state. If it matches the previous
 * raw reading, the counter increments. If it differs (bounce), the
 * counter resets to 1. Once the counter reaches cfg->threshold, the
 * state is confirmed and an edge flag is set if it changed.
 *
 * Returns 0 to keep the 20 ms interval unchanged.
 */
uint32_t task_btn(void) {
    uint8_t raw = (*cfg->pin_reg >> cfg->pin_bit) & 1;

    if (raw == confirmed) {
        count = 0;   /* reading matches confirmed state — no change pending */
        return 0;
    }

    count++;
    if (count >= cfg->threshold) {
        confirmed = raw;
        count = 0;
        if (raw == 0)
            flag_pressed  = 1;  /* LOW = button pressed */
        else
            flag_released = 1;  /* HIGH = button released */
    }

    return 0;
}

/*
 * Returns the current confirmed button state.
 *
 * @return 1 if button is held pressed, 0 if released.
 */
uint8_t btn_is_held(void) {
    return (confirmed == 0);
}

/*
 * Returns 1 exactly once after a confirmed press, then clears automatically.
 * Subsequent calls return 0 until the next confirmed press event.
 *
 * @return 1 on press edge, 0 otherwise.
 */
uint8_t btn_was_pressed(void) {
    if (flag_pressed) {
        flag_pressed = 0;
        return 1;
    }
    return 0;
}

/*
 * Returns 1 exactly once after a confirmed release, then clears automatically.
 *
 * @return 1 on release edge, 0 otherwise.
 */
uint8_t btn_was_released(void) {
    if (flag_released) {
        flag_released = 0;
        return 1;
    }
    return 0;
}
