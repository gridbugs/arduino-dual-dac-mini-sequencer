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
#include "notes.h"

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

int8_t rotary_encoder_position = 0;
rotary_encoder_history_t rotary_encoder_history = {
  .last_rest = REST_01,
  .last_turn = TURN_INIT,
};

uint8_t prev_pind;

volatile uint8_t button_right_count_isr_incremented = 0;
uint8_t button_right_count = 0;

bool button_right_check_and_clear(void) {
  uint8_t button_right_count_isr_incremented_copy = button_right_count_isr_incremented;
  bool ret = button_right_count != button_right_count_isr_incremented_copy;
  button_right_count = button_right_count_isr_incremented_copy;
  return ret;
}

ISR(PCINT2_vect) {
  uint8_t pind = PIND;
  uint8_t rotary_encoder_state = pind >> 6;
  rotary_encoder_position += rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);

  if ((prev_pind & BIT(4)) && !(pind & BIT(4))) {
    button_right_count_isr_incremented++;
  }

  prev_pind = pind;
}

typedef struct {
  bool enabled;
  note_t note;
  uint8_t velocity;
} step_t;

#define NUM_STEPS 16
step_t sequence[NUM_STEPS];

void init_sequence(void) {
  for (int i = 0; i < NUM_STEPS; i++) {
    sequence[i].enabled = false;
  }
}

#define PORTC_GATE_PIN BIT(0)

void gate_on(void) {
  PORTC |= PORTC_GATE_PIN;
}

void gate_off(void) {
  PORTC &= ~PORTC_GATE_PIN;
}

#define PORTC_CLOCK_PIN BIT(1)

ISR(TIMER1_OVF_vect) {
  timer1_stop();
}

ISR(TIMER1_COMPA_vect) {
  gate_on();
}

ISR(TIMER1_COMPB_vect) {
  gate_off();
}

volatile uint8_t clock_rising_edge_count_isr_incremented = 0;
uint8_t clock_rising_edge_count = 0;

bool clock_rising_edge_check_and_clear(void) {
  uint8_t clock_rising_edge_count_isr_incremented_copy = clock_rising_edge_count_isr_incremented;
  bool ret = clock_rising_edge_count != clock_rising_edge_count_isr_incremented_copy;
  clock_rising_edge_count = clock_rising_edge_count_isr_incremented_copy;
  return ret;
}
uint8_t prev_pinc;

ISR(PCINT1_vect) {
  uint8_t pinc = PINC;
  if (!(prev_pinc & PORTC_CLOCK_PIN) && (pinc & PORTC_CLOCK_PIN)) {
    clock_rising_edge_count_isr_incremented++;
  }
  prev_pinc = pinc;
}

#define NOTE_DISPLAY_X 45
#define NOTE_DISPLAY_Y 55
#define NOTE_DISPLAY_FG WHITE
#define NOTE_DISPLAY_BG BLACK

int main(void) {

  timer2_init_pwm_port_d_bit_3(DISPLAY_BACKLIGHT_BRIGHTNESS);

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_bitbanged_init();

  display_init();

  display_clear(MAGENTA);
  display_text("purple", 20, 10, WHITE, BLACK, 1);
  display_text("earth", 30, 30, WHITE, BLACK, 1);
  display_text("hypoth-", 10, 50, WHITE, BLACK, 1);
  display_text("esis", 50, 70, WHITE, BLACK, 1);

  sei();

  DDRD &= ~PORTD_INPUT_PINS;
  PORTD |= PORTD_INPUT_PINS;
  PCICR |= BIT(PCIE2) | BIT(PCIE1);
  PCMSK2 |= PORTD_INPUT_PINS;
  prev_pind = PIND;

  DDRC |= PORTC_GATE_PIN;
  DDRC &= ~PORTC_CLOCK_PIN;
  PCMSK1 |= PORTC_CLOCK_PIN;

  init_sequence();
  timer1_init();

  // Delay before setting the gate after changing notes to account for the
  // delay between sending the I2C message to change the DAC value and the
  // DAC's voltage actually changing.
  timer1_set_output_compare_a(1000);

  // Delay before turning the gate back off.
  timer1_set_output_compare_b(10000);

  printf("Hello, World!\n\r");

  display_clear(BLACK);

  sequence[0] = (step_t) {
    .enabled = false,
  };
  sequence[1] = (step_t) {
    .enabled = true,
    .note = NOTE_C_3,
    .velocity = 127,
  };
  sequence[2] = (step_t) {
    .enabled = true,
    .note = NOTE_C_4,
    .velocity = 127,
  };
  sequence[3] = (step_t) {
    .enabled = true,
    .note = NOTE_C_3,
    .velocity = 255,
  };
  sequence[4] = (step_t) {
    .enabled = true,
    .note = NOTE_D_SHARP_3,
    .velocity = 127,
  };
  sequence[5] = (step_t) {
    .enabled = true,
    .note = NOTE_F_SHARP_3,
    .velocity = 127,
  };
  sequence[6] = (step_t) {
    .enabled = false,
    .note = NOTE_A_3,
    .velocity = 127,
  };
  sequence[7] = (step_t) {
    .enabled = false,
  };

  sequence[8] = (step_t) {
    .enabled = true,
    .note = NOTE_C_4,
    .velocity = 127,
  };
  sequence[9] = (step_t) {
    .enabled = false,
  };
  sequence[10] = (step_t) {
    .enabled = true,
    .note = NOTE_F_SHARP_3,
    .velocity = 255,
  };
  sequence[11] = (step_t) {
    .enabled = true,
    .note = NOTE_A_3,
    .velocity = 255,
  };
  sequence[12] = (step_t) {
    .enabled = false,
  };
  sequence[13] = (step_t) {
    .enabled = true,
    .note = NOTE_D_SHARP_3,
    .velocity = 127,
  };
  sequence[14] = (step_t) {
    .enabled = false,
  };
  sequence[15] = (step_t) {
    .enabled = true,
    .note = NOTE_F_SHARP_3,
    .velocity = 255,
  };


  int sequence_index = 0;
  while (1) {

    step_t step = sequence[sequence_index];
    if (step.enabled) {
      display_text(step.note.name, NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
      dac0_set_value(step.note.dac_value);
      dac1_set_value(step.velocity << 4);

      // Start the timer. A pair of compare interrupts will cause the gate to
      // be set and cleared. This makes it easy to add a delay before setting
      // the gate to account for a delay in the DAC output changinging, and to
      // clear the gate after a fixed period of time.
      timer1_reset_and_start();
    } else {
      display_text(" - ", NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
    }

    sequence_index++;
    if (sequence_index == NUM_STEPS) {
      sequence_index = 0;
    }

    while (!clock_rising_edge_check_and_clear());
  }

  return 0;
}
