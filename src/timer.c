#include <stdint.h>
#include <avr/io.h>
#include "util.h"

void timer1_enable_overflow_interrupt(void) {
    TIMSK1 |= BIT(TOIE1);
}

void timer1_init(void) {
    // Normal mode, clocked at clkIO/1024
    TCCR1A = 0;
    TCCR1B = BIT(CS12);
}

void timer1_reset(void) {
    TCNT1 = 0;
}

uint16_t timer1_read(void) {
    return TCNT1;
}

void timer2_init_pwm_port_d_bit_3(uint8_t duty) {
  DDRD |= BIT(3);
  TCCR2A = BIT(COM2B1) | BIT(WGM21) | BIT(WGM20);
  TCCR2B = BIT(CS22);
  OCR2B = duty;
}
