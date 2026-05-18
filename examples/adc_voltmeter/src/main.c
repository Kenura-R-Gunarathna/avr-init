/*
 * main.c -- ADC Voltmeter Demo (with running-average smoothing)
 *
 * Reads voltage on PC5 (ADC5) and displays it on a 4-digit 7-segment display.
 * Demonstrates the adc driver, which encapsulates ADMUX/ADCSRA setup and
 * performs a running average over a power-of-2 sample window. main.c does
 * not touch raw ADC registers -- the driver handles all of it.
 *
 * Smoothing pipeline:
 *   ADC hardware
 *      |  conversion ~104 us @ 125 kHz ADC clock
 *      v
 *   task_adc     (10 ms)  reads sample, accumulates, divides every 8 samples
 *      |
 *      v latest (volatile uint16_t, averaged 0-1023)
 *   task_display (50 ms)  reads latest, converts to mV, writes display buffer
 *      |
 *      v buf[4] inside ssd4 driver
 *   task_ssd4    (1 ms)   multiplexes one digit per call (250 Hz refresh)
 *
 * Wiring:
 *   Segments a-g, dp    ->  PD7-PD0  (seg_map = {7,6,5,4,3,2,1,0})
 *   Digit selects D0-D3 ->  PC0-PC3  (transistor-buffered, active HIGH)
 *   ADC input           ->  PC5      (100 nF cap to GND recommended)
 *
 * Hardware: ATmega328P @ 1 MHz (internal oscillator, default fuses)
 * Kenura R. Gunarathna -- PH_3032 Embedded Systems
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "drivers/millis.h"
#include "drivers/scheduler.h"
#include "drivers/ssd4.h"
#include "drivers/adc.h"

/*
 * Display wiring + polarity. Change only this block to rewire.
 * seg_active_high = 1: segment pin HIGH lights it.
 * dig_active_high = 1: digit pin HIGH enables it (NPN transistor on cathode).
 */
static const SSD4_Config ssd_cfg = {
    .seg_port        = &PORTD,
    .seg_ddr         = &DDRD,
    .dig_port        = &PORTC,
    .dig_ddr         = &DDRC,
    .dig_pins        = {PC0, PC1, PC2, PC3},
    .seg_map         = {7, 6, 5, 4, 3, 2, 1, 0},
    .seg_active_high = 1,
    .dig_active_high = 1,
};

/*
 * ADC config:
 *   channel   = 5            PC5 / ADC5
 *   ref_avcc  = 1            AVcc (5 V) as reference
 *   prescaler = DIV8         1 MHz / 8 = 125 kHz ADC clock (within 50-200 kHz spec)
 *   avg_shift = 3            2^3 = 8 samples per averaged output
 *
 * Response time = sample_interval * 2^avg_shift = 10 ms * 8 = 80 ms
 * To smooth further: bump avg_shift to 4 (16 samples = 160 ms response time).
 */
static const ADC_Config adc_cfg = {
    .channel   = 5,
    .ref_avcc  = 1,
    .prescaler = ADC_PRESCALER_DIV8,
    .avg_shift = 3,
};

/*
 * Reads the latest averaged ADC value in millivolts and writes it to the
 * display buffer with a decimal point after the leftmost digit.
 * Example: 3.302 V -> shown as "3.302".
 *
 * @return 0  Keep 50 ms interval.
 */
static uint32_t task_display(void) {
    ssd4_number_dp(adc_mv(5000), 3);
    return 0;
}

static struct task tasks[] = {
    { task_ssd4,     1, 0 },   /* display refresh -- first so it wins any tie */
    { task_adc,     10, 0 },   /* one ADC sample per call (avg over 8 samples) */
    { task_display, 50, 0 },   /* render latest averaged mV to digit buffer    */
};

int main(void) {
    adc_init(&adc_cfg);
    ssd4_init(&ssd_cfg);
    millis_init();
    sei();
    scheduler_run(tasks, TASK_COUNT(tasks));
}
