#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "util.h"
#include "uart_bitbanged.h"
#include "adc.h"
#include "timer.h"
#include "mcp4725.h"
#include "display.h"

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x10

// Leave space for TX (bit 1) and PWM (bit 3)
#define PORTD_INPUT_PINS (BIT(0) | BIT(2) | BIT(4) | BIT(5) | BIT(6) | BIT(7))

typedef enum {
  REST_01 = 0,
  REST_10 = 1,
} rotary_encoder_rest_t;

typedef enum {
  TURN_00 = 0,
  TURN_11 = 1,
  TURN_INIT = 2,
} rotary_encoder_turn_t;

typedef struct {
  rotary_encoder_rest_t last_rest;
  rotary_encoder_turn_t last_turn;
} rotary_encoder_history_t;

int8_t rotary_encoder_update(rotary_encoder_history_t *history, uint8_t current_state) {
  switch (current_state) {
    case 0:
      history->last_turn = TURN_00;
      break;
    case 1:
      if (history->last_rest == REST_10) {
        history->last_rest = REST_01;
        if (history->last_turn == TURN_00) {
          return -1;
        } else {
          return 1;
        }
      }
      break;
    case 2:
      if (history->last_rest == REST_01) {
        history->last_rest = REST_10;
        if (history->last_turn == TURN_00) {
          return 1;
        } else {
          return -1;
        }
      }
      break;
    case 3:
      history->last_turn = TURN_11;
      break;
  }
  return 0;
}

int8_t i = 0;
rotary_encoder_history_t rotary_encoder_history = {
  .last_rest = REST_01,
  .last_turn = TURN_INIT,
};

ISR(PCINT2_vect) {
  uint8_t pind = PIND;
  uint8_t rotary_encoder_state = pind >> 6;
  i += rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);
}

int main(void) {

  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_bitbanged_init();

  display_init();

  printf("Hello, World!\n\r");

  display_clear(MAGENTA);
  display_text("purple", 20, 10, WHITE, BLACK, 1);
  display_text("earth", 30, 30, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 50, WHITE, BLACK, 1);
  display_text("esis", 50, 70, WHITE, BLACK, 1);

  display_clear(BLACK);

  sei();

  DDRD &= ~PORTD_INPUT_PINS;
  PORTD |= PORTD_INPUT_PINS;
  PCICR |= BIT(PCIE2);
  PCMSK2 |= PORTD_INPUT_PINS;

  DDRC |= BIT(0);
  PORTC |= BIT(0);


  uint16_t v = 0;
  while (1) {
//    char text[] = { 'a' + i, 0 };
//    display_text(text, 50, 50, WHITE, BLACK, 1);
    dac0_set_value(v);
    dac1_set_value(v);
    v += 1;
    if (v >= 4080) {
      v = 0;
    }
  }

  return 0;
}
