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

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x20

#define PORTD_ENCODER_A BIT(7)
#define PORTD_ENCODER_B BIT(6)
#define PORTD_ENCODER_BUTTON_PIN BIT(5)
#define PORTD_BUTTON_RIGHT_PIN BIT(4)
#define PORTD_BUTTON_MIDDLE_PIN BIT(2)
#define PORTD_BUTTON_LEFT_PIN BIT(0)

// Leave space for TX (bit 1) and PWM (bit 3)
#define PORTD_INPUT_PINS ( \
    PORTD_ENCODER_A | \
    PORTD_ENCODER_B | \
    PORTD_ENCODER_BUTTON_PIN | \
    PORTD_BUTTON_RIGHT_PIN | \
    PORTD_BUTTON_MIDDLE_PIN | \
    PORTD_BUTTON_LEFT_PIN )

volatile bool programming_mode = false;
volatile bool internal_clock = true;

// Unit is ms added to the iteration time of the main loop (which is pretty small!).
uint16_t internal_clock_delay = 100;

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

typedef struct {
  volatile uint8_t count_setter_incremented;
  uint8_t count_to_check;
} async_flag_t;

void async_flag_set(async_flag_t *async_flag) {
  async_flag->count_setter_incremented++;
}

bool async_flag_check_and_clear(async_flag_t *async_flag) {
  uint8_t count_copy = async_flag->count_setter_incremented;
  bool ret = count_copy != async_flag->count_to_check;
  async_flag->count_to_check = count_copy;
  return ret;
}

async_flag_t left_button = { 0 };

ISR(PCINT2_vect) {
  uint8_t pind = PIND;
  uint8_t rotary_encoder_state = pind >> 6;
  int8_t rotary_encoder_delta = rotary_encoder_update(&rotary_encoder_history, rotary_encoder_state);
  if (programming_mode) {
    rotary_encoder_position += rotary_encoder_delta;
  } else if (internal_clock) {
    int16_t internal_clock_delay_signed = (int16_t)internal_clock_delay + ((int16_t)rotary_encoder_delta) * 10;
    if (internal_clock_delay_signed >= 0) {
      internal_clock_delay = (uint16_t)internal_clock_delay_signed;
    }
  }

  if ((prev_pind & PORTD_BUTTON_RIGHT_PIN) && !(pind & PORTD_BUTTON_RIGHT_PIN)) {
    if (!programming_mode) {
      internal_clock = !internal_clock;
    }
  }
  if ((prev_pind & PORTD_BUTTON_MIDDLE_PIN) && !(pind & PORTD_BUTTON_MIDDLE_PIN)) {
    programming_mode = !programming_mode;
  }
  if ((prev_pind & PORTD_BUTTON_LEFT_PIN) && !(pind & PORTD_BUTTON_LEFT_PIN)) {
    async_flag_set(&left_button);
  }

  prev_pind = pind;
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

async_flag_t clock_rising_edge = { 0 };

uint8_t prev_pinc;

ISR(PCINT1_vect) {
  uint8_t pinc = PINC;
  if (!(prev_pinc & PORTC_CLOCK_PIN) && (pinc & PORTC_CLOCK_PIN)) {
    async_flag_set(&clock_rising_edge);
  }
  prev_pinc = pinc;
}

#define NOTE_DISPLAY_X 45
#define NOTE_DISPLAY_Y 55
#define NOTE_DISPLAY_FG WHITE
#define NOTE_DISPLAY_BG BLACK

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

char progress_buffer[6] = { 0 };
char status_buffer[3] = { 0 };
int sequence_index = 0;

void display_current_note(void) {
  step_t step = sequence[sequence_index];
  if (step.enabled) {
    display_text(step.note.name, NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
  } else {
    display_text(" - ", NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);
  }
}

void display_bottom_line(void) {
  sprintf(progress_buffer, "%02d/%d", sequence_index, NUM_STEPS);
  display_text(progress_buffer, DISPLAY_WIDTH - 5 * 8, DISPLAY_HEIGHT - 8, BLACK, WHITE, 0);

  if (programming_mode) {
    status_buffer[0] = 'P';
  } else {
    status_buffer[0] = 'R';
  }
  if (internal_clock) {
    status_buffer[1] = 'I';
  } else {
    status_buffer[1] = 'E';
  }
  display_text(status_buffer, 0, DISPLAY_HEIGHT - 8, BLACK, WHITE, 0);
}

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


  display_text("...", NOTE_DISPLAY_X, NOTE_DISPLAY_Y, NOTE_DISPLAY_FG, NOTE_DISPLAY_BG, 1);

  while (1) {
    if (programming_mode) {
      display_current_note();
      display_bottom_line();
    } else {
      while (true) {
        if (programming_mode) {
          break;
        } else if (internal_clock) {
          delay_ms(internal_clock_delay);
          break;
        } else if (async_flag_check_and_clear(&clock_rising_edge)) {
          break;
        }
      }
      if (programming_mode) {
        continue;
      }

      if (async_flag_check_and_clear(&left_button)) {
        sequence_index = 0;
      }

      step_t step = sequence[sequence_index];
      if (step.enabled) {
        dac0_set_value(step.note.dac_value);
        dac1_set_value(step.velocity << 4);

        // Start the timer. A pair of compare interrupts will cause the gate to
        // be set and cleared. This makes it easy to add a delay before setting
        // the gate to account for a delay in the DAC output changinging, and to
        // clear the gate after a fixed period of time.
        timer1_reset_and_start();
      }

      display_current_note();
      display_bottom_line();

      sequence_index++;
      if (sequence_index == NUM_STEPS) {
        sequence_index = 0;
      }
    }
  }

  return 0;
}
