#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include "drivers/millis.h"

int main(void) {
    DDRB |= (1 << PB5) | (1 << PB4) | (1 << PB3);     // LED A = PB5, LED B = PB4, LED C = PB3
    millis_init();
    sei();

    uint32_t lastA = 0, lastB = 0, lastC = 0;
    uint32_t nextWaitC = 800; // Start with the OFF duration

    for (;;) {
        uint32_t now = millis();

        // Task A: 100 ms ; 50% duty cycle ; runs every 100 ms ; T = 200ms
        if ((uint32_t)(now - lastA) >= 100) {
            PINB = (1 << PB5);
            lastA = now;
        }

        // Task B: 500 ms ; 50% duty cycle ; runs every 500 ms ; T = 1s
        if ((uint32_t)(now - lastB) >= 500) {
            PINB = (1 << PB4);
            lastB = now;
        }

        // Task C: 1s ; 50% duty cycle ; runs every 1s ; T = 2s
        if ((uint32_t)(now - lastC) >= nextWaitC) {
            PORTB ^= (1 << PB3); // XOR to toggle the pin
            lastC = now;

            // Check if the pin is now HIGH or LOW to set the NEXT wait time
            if (PORTB & (1 << PB3)) {
                nextWaitC = 200;  // It's HIGH, so stay high for 200ms
            } else {
                nextWaitC = 800;  // It's LOW, so stay low for 800ms
            }
        }
    }
}
