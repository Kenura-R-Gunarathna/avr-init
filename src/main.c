/*
 * main.c -- SSD4 Voltmeter + Button Demo
 *
 * Reads voltage on PC5 (ADC5) and displays it in volts on a 4-digit 7-segment
 * display. A push button on PB0 lights an LED on PB1 while pressed (with
 * proper debounce via the btn driver). Polarity flags configured for a board
 * with segments active HIGH and digit selects active HIGH (transistor-buffered).
 *
 * Wiring:
 *   Segments a-g, dp    ->  PD7-PD0
 *   Digit selects D0-D3 ->  PC0-PC3
 *   ADC input           ->  PC5 (analog, no pull-up)
 *   Button              ->  PB0 to GND (internal pull-up enabled)
 *   LED                 ->  PB1 through 220 ohm to GND
 *
 * Task schedule:
 *   task_ssd4   1 ms   multiplexes one digit per call (250 Hz refresh)
 *   task_adc   50 ms   reads completed ADC result, starts next conversion
 *   task_btn   20 ms   samples + debounces button input
 *   task_led   20 ms   responds to confirmed press/release edge events
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
 * Acts on confirmed button edges from the btn driver.
 * btn_was_pressed() / btn_was_released() are one-shot — they fire exactly once
 * per confirmed edge and self-clear after being read.
 *
 * @return 0  Keep 20 ms interval.
 */
static uint32_t task_led(void) {
    if (btn_was_pressed())
        PORTB |=  (1 << PB1);   /* confirmed press  -> LED on  */
    if (btn_was_released())
        PORTB &= ~(1 << PB1);   /* confirmed release -> LED off */
    return 0;
}

/*
 * Reads the most recently completed ADC conversion, converts to millivolts,
 * updates the display, then starts the next conversion.
 * Non-blocking: conversion takes ~104 us at 125 kHz ADC clock; by the time
 * this task fires again (50 ms later) the result is long ready.
 *
 * @return 0  Keep 50 ms interval.
 */
static uint32_t task_adc(void) {
    uint16_t mv = (uint32_t)ADC * 5000 / 1024;
    ssd4_number_dp(mv, 3);
    ADCSRA |= (1 << ADSC);
    return 0;
}

static struct task tasks[] = {
    { task_ssd4,  1, 0 },   /* display refresh -- first so it wins any tie  */
    { task_adc,  50, 0 },   /* ADC read + display update                    */
    { task_btn,  20, 0 },   /* debounce sampling                            */
    { task_led,  20, 0 },   /* react to confirmed button edges              */
};

int main(void) {
    /* LED setup -- button pin is configured by btn_init() */
    DDRB  |=  (1 << PB1);          /* PB1 output (LED)      */
    PORTB &= ~(1 << PB1);          /* LED off initially     */

    /* ADC setup -- PC5 as analog input, AVcc ref, ADC5 channel, /8 prescaler */
    DDRC  &= ~(1 << PC5);
    PORTC &= ~(1 << PC5);
    ADMUX  = (1 << REFS0) | (1 << MUX2) | (1 << MUX0);
    ADCSRA = (1 << ADEN)  | (1 << ADPS1) | (1 << ADPS0);

    /* Discard first conversion -- reference capacitor needs one cycle to settle */
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    ADCSRA |= (1 << ADSC);         /* start first real conversion */

    btn_init(&btn_cfg);
    ssd4_init(&ssd_cfg);
    millis_init();
    sei();
    scheduler_run(tasks, TASK_COUNT(tasks));
}
