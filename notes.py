print(
    """
#include "note.h"

#define DAC_VALUE_PER_SEMITONE 34
#define NOTE(i, note_name) { .name = note_name, .dac_value = i * DAC_VALUE_PER_SEMITONE }

static const note_t notes[NUM_NOTES] = {
"""
)

note_names = [
    "C ",
    "C#",
    "D ",
    "D#",
    "E ",
    "F ",
    "F#",
    "G ",
    "G#",
    "A ",
    "A#",
    "B ",
]

num_octaves = 10

i = 0

for octave in range(0, num_octaves):
    for note_name in note_names:
        print('  NOTE(%d, "%s%d"),' % (i, note_name, octave))
        i += 1

print(
    """
};

note_t get_note(uint8_t i) {
  return notes[i];
}
"""
)
