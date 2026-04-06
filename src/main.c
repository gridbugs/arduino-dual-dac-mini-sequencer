#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "util.h"
#include "uart_bitbanged.h"
#include "adc.h"
#include "timer.h"
#include "mcp4725.h"
#include "display.h"

#define DISPLAY_BACKLIGHT_BRIGHTNESS 0x10

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

  while (1) {}

  return 0;
}
