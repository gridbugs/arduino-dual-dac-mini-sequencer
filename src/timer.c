#include <stdint.h>
#include <avr/io.h>
#include "util.h"

void timer_enable_overflow_interrupt(void) {
    TIMSK1 |= BIT(TOIE1);
}

void timer_init(void) {
    // Normal mode, clocked at clkIO/1024
    TCCR1A = 0;
    TCCR1B = BIT(CS12);
}

void timer_reset(void) {
    TCNT1 = 0;
}

uint16_t timer_read(void) {
    return TCNT1;
}
