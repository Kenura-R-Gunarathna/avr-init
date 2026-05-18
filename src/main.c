/*
 * main.c -- SSD4 Voltmeter + Button Demo (averaged ADC)
 *
 * Reads voltage on PC5 (ADC5) and displays it in volts on a 4-digit 7-segment
 * display. ADC samples are averaged (8 samples, ~80 ms response) by the adc
 * driver so the reading stays stable. A push button on PB0 lights an LED on
 * PB1 while pressed, with software debounce by the btn driver.
 * Polarity flags configured for a board with segments active HIGH and digit
 * selects active HIGH (transistor-buffered).
 *
 * Wiring:
 *   Segments a-g, dp    ->  PD7-PD0
 *   Digit selects D0-D3 ->  PC0-PC3
 *   ADC input           ->  PC5 (analog, 100 nF cap to GND for noise filtering)
 *   Button              ->  PB0 to GND (internal pull-up enabled)
 *   LED                 ->  PB1 through 220 ohm to GND
 *
 * Task schedule:
 *   task_ssd4      1 ms  multiplexes one digit per call (250 Hz refresh)
 *   task_adc      10 ms  takes one ADC sample, averages over 8 samples
 *   task_display  50 ms  reads latest averaged mv, writes to display buffer
 *   task_btn      20 ms  samples + debounces button input
 *   task_led      20 ms  responds to confirmed press/release edge events
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
#include "drivers/btn.h"
#include "drivers/adc.h"

/*
 * Display wiring + polarity. Change only this block to rewire.
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
 * Button wiring + debounce threshold. At 20 ms poll, threshold 3 = 60 ms confirm.
 */
static const BTN_Config btn_cfg = {
    .pin_reg   = &PINB,
    .ddr_reg   = &DDRB,
    .port_reg  = &PORTB,
    .pin_bit   = PB0,
    .threshold = 3,
};

/*
 * ADC config: PC5 channel, AVcc reference, /8 prescaler (125 kHz ADC clock),
 * 8-sample running average (2^3).
 */
static const ADC_Config adc_cfg = {
    .channel   = 5,
    .ref_avcc  = 1,
    .prescaler = ADC_PRESCALER_DIV8,
    .avg_shift = 3,
};

/*
 * Acts on confirmed button edges from the btn driver.
 *
 * @return 0  Keep 20 ms interval.
 */
static uint32_t task_led(void) {
    if (btn_was_pressed())  PORTB |=  (1 << PB1);
    if (btn_was_released()) PORTB &= ~(1 << PB1);
    return 0;
}

/*
 * Reads the latest averaged ADC value in millivolts and updates the display.
 * The adc driver does the sampling and averaging in its own task; this one
 * just renders the latest stable value.
 *
 * @return 0  Keep 50 ms interval.
 */
static uint32_t task_display(void) {
    ssd4_number_dp(adc_mv(5000), 3);
    return 0;
}

static struct task tasks[] = {
    { task_ssd4,     1, 0 },   /* display refresh -- first so it wins any tie */
    { task_adc,     10, 0 },   /* sample ADC, 8 samples = 80 ms response time */
    { task_display, 50, 0 },   /* render averaged value to the digit buffer   */
    { task_btn,     20, 0 },   /* debounce sampling                           */
    { task_led,     20, 0 },   /* react to confirmed button edges             */
};

int main(void) {
    /* LED setup -- button pin is configured by btn_init(), ADC pin by adc_init() */
    DDRB  |=  (1 << PB1);
    PORTB &= ~(1 << PB1);

    adc_init(&adc_cfg);
    btn_init(&btn_cfg);
    ssd4_init(&ssd_cfg);
    millis_init();
    sei();
    scheduler_run(tasks, TASK_COUNT(tasks));
}
