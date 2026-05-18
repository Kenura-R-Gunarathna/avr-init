#pragma once

/*
 * ssd4.h — 4-digit 7-segment display driver
 *
 * Supports any combination of MCU-level polarity for segments and digit selects.
 * This handles direct-drive common anode, direct-drive common cathode, and
 * transistor-buffered configurations (which often invert the digit polarity).
 *
 * Pin mapping, segment wiring, and polarity are configured entirely in
 * main.c via SSD4_Config. The driver never references specific ports directly.
 *
 * Usage:
 *   1. Define a SSD4_Config in main.c with your wiring + polarity flags.
 *   2. Call ssd4_init() once before the scheduler starts.
 *   3. Register task_ssd4 at 1ms in the scheduler task array.
 *   4. Call ssd4_number() / ssd4_hex() etc. from other tasks to update display.
 *
 * Polarity cheat sheet:
 *   Direct-drive common anode   : seg_active_high=0, dig_active_high=1
 *   Direct-drive common cathode : seg_active_high=1, dig_active_high=0
 *   CC + NPN digit transistors  : seg_active_high=1, dig_active_high=1
 *   CA + PNP digit transistors  : seg_active_high=0, dig_active_high=0
 */

#include <stdint.h>
#include <avr/io.h>

/*
 * Wiring configuration — fill this in main.c, pass pointer to ssd4_init().
 *
 * seg_map maps each segment (a–g, dp) to the physical bit number on seg_port.
 * Index order: {a, b, c, d, e, f, g, dp}
 * Example — a on PD7, b on PD6 ... dp on PD0: seg_map = {7,6,5,4,3,2,1,0}
 *
 * seg_active_high : 1 if segment pin HIGH turns the segment ON.
 * dig_active_high : 1 if digit select pin HIGH enables the digit.
 */
typedef struct {
    volatile uint8_t *seg_port;   /* port register for segment pins (e.g. &PORTD) */
    volatile uint8_t *seg_ddr;    /* DDR register for segment pins  (e.g. &DDRD)  */
    volatile uint8_t *dig_port;   /* port register for digit selects (e.g. &PORTC) */
    volatile uint8_t *dig_ddr;    /* DDR register for digit selects  (e.g. &DDRC)  */
    uint8_t dig_pins[4];          /* bit positions for D0–D3 on dig_port             */
    uint8_t seg_map[8];           /* bit positions for a,b,c,d,e,f,g,dp on seg_port  */
    uint8_t seg_active_high;      /* 1 = segment pin HIGH lights it, 0 = LOW lights it */
    uint8_t dig_active_high;      /* 1 = digit pin HIGH enables it,  0 = LOW enables it */
} SSD4_Config;

/*
 * Initialise the driver. Sets DDR, blanks the display, and precomputes the
 * hardware-specific segment patterns from the logical table.
 * Call once in main() before millis_init() / sei() / scheduler_run().
 *
 * @param cfg  Pointer to a SSD4_Config describing your wiring.
 */
void ssd4_init(const SSD4_Config *cfg);

/*
 * Scheduler task — multiplexes one digit per call.
 * Register at 1 ms interval as the first entry in the task array.
 * 4 digits × 1 ms = 4 ms full cycle = 250 Hz refresh rate.
 * Always returns 0 (keep 1 ms interval).
 */
uint32_t task_ssd4(void);

/*
 * Display a decimal number 0–9999.
 * Values above 9999 are clamped to 9999.
 * buf[0] = ones (rightmost), buf[3] = thousands (leftmost).
 *
 * @param n  Value to display.
 */
void ssd4_number(uint16_t n);

/*
 * Display a decimal number 0–9999 with a decimal point on one digit.
 * Example: ssd4_number_dp(3302, 3) shows "3.302" (voltage 3.302 V).
 * dp_pos 0 = ones (rightmost), 3 = thousands (leftmost).
 *
 * @param n       Value to display.
 * @param dp_pos  Digit position (0–3) that gets the decimal point.
 */
void ssd4_number_dp(uint16_t n, uint8_t dp_pos);

/*
 * Display a 16-bit value in hexadecimal (0000–FFFF).
 * Useful for debugging register values or ADC raw readings.
 *
 * @param n  Value to display.
 */
void ssd4_hex(uint16_t n);

/*
 * Write a raw segment pattern to one digit position.
 * Use for characters not in 0–F (e.g. minus sign, custom glyphs).
 * Accepts a logical pattern: bit 7 = a, bit 6 = b ... bit 0 = dp.
 * The driver remaps to your physical wiring automatically.
 * Example — minus sign (segment g only): ssd4_raw(2, 0x02)
 *
 * @param pos      Digit position (0 = rightmost, 3 = leftmost).
 * @param logical  Segment pattern in canonical bit order (a=7 ... dp=0).
 */
void ssd4_raw(uint8_t pos, uint8_t logical);

/*
 * Turn off all digits immediately.
 * Sets buf[] to all-OFF state without touching hardware directly
 * (task_ssd4 picks it up on the next 1 ms tick).
 */
void ssd4_blank(void);
