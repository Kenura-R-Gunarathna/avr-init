#include "ssd4.h"
#include <avr/pgmspace.h>

/*
 * Canonical segment patterns for 0–F.
 * Bit order: a=7, b=6, c=5, d=4, e=3, f=2, g=1, dp=0  (1 = segment ON).
 * Stored in PROGMEM (flash) to save 16 bytes of RAM.
 * Read once at init via pgm_read_byte() to build remapped[].
 */
static const uint8_t seg7[] PROGMEM = {
    0xFC, // 0
    0x60, // 1
    0xDA, // 2
    0xF2, // 3
    0x66, // 4
    0xB6, // 5
    0xBE, // 6
    0xE0, // 7
    0xFE, // 8
    0xF6, // 9
    0xEE, // A
    0x3E, // b
    0x9C, // C
    0x7A, // d
    0x9E, // E
    0x8E, // F
};

static uint8_t           remapped[16];  /* hardware-ready patterns, precomputed at init   */
static volatile uint8_t  buf[4];        /* current pattern for each digit (written to port) */
static uint8_t           cur;           /* digit index currently being driven (0–3)       */
static uint8_t           dp_mask;       /* physical bit mask for the decimal point pin    */
static uint8_t           seg_blank;     /* hardware value for all-segments-OFF            */
static const SSD4_Config *cfg;          /* pointer to user wiring config from main.c      */

/*
 * Initialises the driver.
 *
 * Precomputes remapped[] by translating each canonical pattern from seg7[]
 * into a hardware-specific byte that can be written directly to seg_port.
 * Polarity is determined by cfg->seg_active_high:
 *   seg_active_high = 1 : segment ON = pin HIGH → blank=0x00, set bit to light
 *   seg_active_high = 0 : segment ON = pin LOW  → blank=0xFF, clear bit to light
 * Running this loop once at startup means task_ssd4() has zero per-call overhead.
 *
 * @param c  Pointer to a SSD4_Config with wiring details.
 */
void ssd4_init(const SSD4_Config *c) {
    cfg = c;

    seg_blank = cfg->seg_active_high ? 0x00 : 0xFF;

    for (uint8_t d = 0; d < 16; d++) {
        uint8_t logical  = pgm_read_byte(&seg7[d]);
        uint8_t physical = seg_blank;
        for (uint8_t seg = 0; seg < 8; seg++) {
            if (logical & (1 << (7 - seg))) {
                if (cfg->seg_active_high)
                    physical |=  (1 << cfg->seg_map[seg]); /* HIGH = ON */
                else
                    physical &= ~(1 << cfg->seg_map[seg]); /* LOW  = ON */
            }
        }
        remapped[d] = physical;
    }

    dp_mask = (1 << cfg->seg_map[7]);

    *cfg->seg_ddr  = 0xFF;
    *cfg->seg_port = seg_blank;   /* all segments OFF at startup */
    for (uint8_t i = 0; i < 4; i++) {
        *cfg->dig_ddr  |=  (1 << cfg->dig_pins[i]);
        /*
         * dig_active_high = 1 : digit ON = pin HIGH → init LOW  (digit OFF)
         * dig_active_high = 0 : digit ON = pin LOW  → init HIGH (digit OFF)
         */
        if (cfg->dig_active_high)
            *cfg->dig_port &= ~(1 << cfg->dig_pins[i]);
        else
            *cfg->dig_port |=  (1 << cfg->dig_pins[i]);
    }

    ssd4_blank();
}

/*
 * Turns off all digits by setting buf[] to the all-segments-OFF hardware value.
 * task_ssd4() picks up the new buf[] on the next 1 ms tick.
 */
void ssd4_blank(void) {
    for (uint8_t i = 0; i < 4; i++) buf[i] = seg_blank;
}

/*
 * Multiplexes one digit per call. Must be registered in the scheduler at 1 ms.
 *
 * Sequence per call — order is critical to prevent ghosting:
 *   1. Disable all digit select pins (blank the display briefly).
 *   2. Write the new segment pattern to seg_port.
 *   3. Enable only the current digit select pin.
 *
 * Returns 0 to keep the 1 ms interval unchanged.
 */
uint32_t task_ssd4(void) {
    /* step 1: disable all digits to prevent ghosting */
    for (uint8_t i = 0; i < 4; i++) {
        if (cfg->dig_active_high)
            *cfg->dig_port &= ~(1 << cfg->dig_pins[i]); /* LOW  = digit OFF */
        else
            *cfg->dig_port |=  (1 << cfg->dig_pins[i]); /* HIGH = digit OFF */
    }

    *cfg->seg_port = buf[cur];  /* step 2: write segment pattern */

    /* step 3: enable current digit only */
    if (cfg->dig_active_high)
        *cfg->dig_port |=  (1 << cfg->dig_pins[cur]); /* HIGH = digit ON */
    else
        *cfg->dig_port &= ~(1 << cfg->dig_pins[cur]); /* LOW  = digit ON */

    cur = (cur + 1) & 3;
    return 0;
}

/*
 * Converts a decimal value 0–9999 into four digit patterns and stores them
 * in buf[]. Values above 9999 are clamped.
 * buf[0] = ones (rightmost digit), buf[3] = thousands (leftmost digit).
 *
 * @param n  Value to display.
 */
void ssd4_number(uint16_t n) {
    if (n > 9999) n = 9999;
    buf[0] = remapped[n % 10];
    buf[1] = remapped[(n / 10)  % 10];
    buf[2] = remapped[(n / 100) % 10];
    buf[3] = remapped[n / 1000];
}

/*
 * Same as ssd4_number() but turns on the decimal point for one digit.
 * Modifies the dp pin bit in buf[dp_pos] according to seg_active_high.
 * Example: ssd4_number_dp(3302, 3) → shows "3.302" for a 3.302 V reading.
 *
 * @param n       Value to display (0–9999).
 * @param dp_pos  Digit that gets the decimal point (0 = ones, 3 = thousands).
 */
void ssd4_number_dp(uint16_t n, uint8_t dp_pos) {
    ssd4_number(n);
    if (dp_pos < 4) {
        if (cfg->seg_active_high)
            buf[dp_pos] |=  dp_mask;  /* HIGH = dp ON */
        else
            buf[dp_pos] &= ~dp_mask;  /* LOW  = dp ON */
    }
}

/*
 * Displays a 16-bit value in hexadecimal (0000–FFFF).
 * Useful for debugging raw register or ADC values.
 *
 * @param n  Value to display.
 */
void ssd4_hex(uint16_t n) {
    buf[0] = remapped[n         & 0xF];
    buf[1] = remapped[(n >> 4)  & 0xF];
    buf[2] = remapped[(n >> 8)  & 0xF];
    buf[3] = remapped[n >> 12];
}

/*
 * Writes a custom segment pattern to one digit position.
 * For characters not in 0–F (minus sign, degree symbol, custom glyphs).
 * Accepts a logical pattern in canonical bit order (a=7, b=6 ... dp=0)
 * and remaps it to the physical wiring automatically.
 * Example — minus sign (segment g only, bit 1): ssd4_raw(2, 0x02)
 *
 * @param pos      Digit position (0 = rightmost, 3 = leftmost).
 * @param logical  Segment pattern: bit 7 = a, bit 6 = b ... bit 0 = dp.
 */
void ssd4_raw(uint8_t pos, uint8_t logical) {
    if (pos >= 4) return;
    uint8_t physical = seg_blank;
    for (uint8_t seg = 0; seg < 8; seg++) {
        if (logical & (1 << (7 - seg))) {
            if (cfg->seg_active_high)
                physical |=  (1 << cfg->seg_map[seg]);
            else
                physical &= ~(1 << cfg->seg_map[seg]);
        }
    }
    buf[pos] = physical;
}
