#include <stdint.h>
#include "st7735.h"
#include "display.h"

void display_init(void) {
  st7735_init();
}

void display_fill_window(window_t window, uint16_t colour) {
  st7735_prepare_for_window(window);
  for (int i = 0; i < (window.w * window.h); i++) {
    st7735_send_colour(colour);
  }
  st7735_finalize();
}

void display_clear(uint16_t colour) {
  display_fill_window((window_t) { .x = 0, .y = 0, .w = DISPLAY_WIDTH, .h = DISPLAY_HEIGHT }, colour);
}
