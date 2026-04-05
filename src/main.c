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


int main(void) {

  timer2_init_pwm_port_d_bit_3(0x80);

  // Allow printing over UART. The UART pins double up as digital IO pins so this
  // will mess with functionality, but handy in emergencies.
  USART0_bitbanged_init();

  display_init();

  printf("Hello, World!\n\r");

  while (1) {}

  return 0;
}
