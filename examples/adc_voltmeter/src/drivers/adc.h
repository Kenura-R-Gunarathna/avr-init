#pragma once

/*
 * adc.h -- ADC driver with running average
 *
 * Configures ADMUX / ADCSRA, samples the ADC one reading per scheduler tick,
 * and maintains a running average over a power-of-2 sample window.
 * Window divide is done as a right shift, so no division at runtime.
 *
 * Usage:
 *   1. Define an ADC_Config in main.c.
 *   2. Call adc_init() once before the scheduler starts.
 *   3. Register task_adc at a fast interval (e.g. 10 ms).
 *   4. Read with adc_value() (raw 0-1023) or adc_mv(vref) (millivolts).
 */

#include <stdint.h>
#include <avr/io.h>

/* ADC clock prescaler -- pick one so ADC clock lands in 50-200 kHz */
#define ADC_PRESCALER_DIV2   ((1 << ADPS0))
#define ADC_PRESCALER_DIV4   ((1 << ADPS1))
#define ADC_PRESCALER_DIV8   ((1 << ADPS1) | (1 << ADPS0))
#define ADC_PRESCALER_DIV16  ((1 << ADPS2))
#define ADC_PRESCALER_DIV32  ((1 << ADPS2) | (1 << ADPS0))
#define ADC_PRESCALER_DIV64  ((1 << ADPS2) | (1 << ADPS1))
#define ADC_PRESCALER_DIV128 ((1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0))

/*
 * @param channel    ADC channel 0-5 (PC0..PC5 on ATmega328P)
 * @param ref_avcc   1 = AVcc reference, 0 = external AREF
 * @param prescaler  one of the ADC_PRESCALER_* macros above
 * @param avg_shift  log2 of averaging window (e.g. 3 = 8 samples, 4 = 16 samples)
 */
typedef struct {
    uint8_t channel;
    uint8_t ref_avcc;
    uint8_t prescaler;
    uint8_t avg_shift;
} ADC_Config;

/*
 * Configures the ADC pin, ADMUX, ADCSRA, and discards the first warm-up
 * conversion. Starts the first real conversion so task_adc can read it.
 *
 * @param cfg  Pointer to an ADC_Config.
 */
void adc_init(const ADC_Config *cfg);

/*
 * Scheduler task -- one sample per call. Reads the previous conversion,
 * accumulates it, starts the next conversion. When the window fills, the
 * averaged result is published and accumulator resets.
 * Returns 0 to keep its scheduled interval.
 */
uint32_t task_adc(void);

/*
 * Returns the latest averaged ADC reading (0-1023).
 */
uint16_t adc_value(void);

/*
 * Returns the latest averaged reading converted to millivolts.
 * @param vref_mv  Reference voltage in millivolts (5000 for AVcc at 5 V).
 */
uint16_t adc_mv(uint16_t vref_mv);
