#include "adc.h"

static const ADC_Config *cfg;
static uint32_t          accumulator;
static uint8_t           sample_count;
static uint8_t           samples_target;
static volatile uint16_t latest;

/*
 * Configures the ADC according to the user config struct.
 *
 * - Sets the selected channel pin as input with no pull-up.
 * - Disables the digital input buffer on the ADC pin (DIDR0) to reduce
 *   noise pickup and save power -- the analog comparator inside the ADC
 *   does not need the digital schmitt trigger.
 * - Programs ADMUX (reference + channel) and ADCSRA (enable + prescaler).
 * - Discards one warm-up conversion so the reference cap settles.
 * - Starts the first real conversion so the first task_adc() call has data.
 *
 * @param c  Pointer to an ADC_Config.
 */
void adc_init(const ADC_Config *c) {
    cfg            = c;
    samples_target = (1 << cfg->avg_shift);
    accumulator    = 0;
    sample_count   = 0;
    latest         = 0;

    if (cfg->channel <= 5) {
        DDRC  &= ~(1 << cfg->channel);
        PORTC &= ~(1 << cfg->channel);
        DIDR0 |=  (1 << cfg->channel);
    }

    ADMUX  = (cfg->ref_avcc ? (1 << REFS0) : 0) | (cfg->channel & 0x0F);
    ADCSRA = (1 << ADEN) | (cfg->prescaler & 0x07);

    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));   /* warm-up conversion discarded */

    ADCSRA |= (1 << ADSC);          /* first real conversion */
}

/*
 * Samples once per call. Returns immediately if the previous conversion is
 * still running (should not happen at reasonable task intervals).
 *
 * Running-average algorithm:
 *   accumulator += new_sample
 *   sample_count++
 *   if (sample_count == 2^N):
 *       latest = accumulator >> N       // cheap divide, since N is power of 2
 *       accumulator = 0, sample_count = 0
 *
 * Returns 0 to keep the scheduled interval.
 */
uint32_t task_adc(void) {
    if (ADCSRA & (1 << ADSC)) return 0;   /* still converting -- skip this tick */

    accumulator += ADC;
    sample_count++;

    if (sample_count >= samples_target) {
        latest       = (uint16_t)(accumulator >> cfg->avg_shift);
        accumulator  = 0;
        sample_count = 0;
    }

    ADCSRA |= (1 << ADSC);   /* start next conversion */
    return 0;
}

/*
 * Returns the latest published averaged value (0-1023).
 */
uint16_t adc_value(void) {
    return latest;
}

/*
 * Converts latest averaged value to millivolts given the reference voltage.
 * 1024 used as denominator (10-bit full scale).
 *
 * @param vref_mv  Reference voltage in mV (5000 for AVcc at 5 V).
 */
uint16_t adc_mv(uint16_t vref_mv) {
    return (uint16_t)(((uint32_t)latest * vref_mv) / 1024);
}
