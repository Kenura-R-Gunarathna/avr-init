#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "drivers/millis.h"

int main(void) {
    DDRB |= (1 << PB5) | (1 << PB4);     // LED A = PB5, LED B = PB4
    millis_init();
    sei();

    uint32_t lastA = 0, lastB = 0;

    for (;;) {
        uint32_t now = millis();

        if ((uint32_t)(now - lastA) >= 100) {     // Task A: 100 ms
            PINB = (1 << PB5);
            lastA = now;
        }
        if ((uint32_t)(now - lastB) >= 500) {     // Task B: 500 ms
            PINB = (1 << PB4);
            lastB = now;
        }
    }
}
