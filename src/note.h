#pragma once

#include <stdint.h>

#define NUM_NOTES 120

typedef struct {
  const char* name;
  uint16_t dac_value;
} note_t;
