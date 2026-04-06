print(
    """
#pragma once
#include "note.h"
"""
)

dac_value_per_semitone = 32

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
        note_to_display = "{note_name}{octave}".format(
            note_name=note_name, octave=octave
        )
        note_for_constant_name = note_to_display.replace("#", "_SHARP_").replace(
            " ", "_"
        )
        dac_value = i * dac_value_per_semitone
        print(
            '#define NOTE_{note_for_constant_name} ((note_t) {{ .name = "{name_to_display}", .dac_value = {dac_value} }})'.format(
                note_for_constant_name=note_for_constant_name,
                name_to_display=note_to_display,
                dac_value=dac_value,
            )
        )
        i += 1
