#!/usr/bin/env python3
"""Exercise the fixed voice manager through the public processor API.

Voice assignments are owned by Gin's audio-thread manager. This example checks
bounded voice stealing using lower-zone MPE member channels.
"""

import numpy as np
import resonarium


SAMPLE_RATE = 48_000
BLOCK_SIZE = 128
VOICE_LIMIT = 4


def process_one_block(synth):
    audio = synth.create_multi_block(1)
    synth.process_multi_block(audio, 0, 1)
    return np.asarray(audio)


synth = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)
synth.set_param("numVoices", float(VOICE_LIMIT))
synth.set_param("impExciter0 enabled", 1.0)
synth.set_param("enabled wb0r0", 1.0)

# Establish four rendered assignments first, so replacements use the two
# bounded 3 ms overlap slots rather than simply reusing unrendered voices.
for index, note in enumerate((60, 64, 67, 71)):
    member_channel = 1 + index  # Python is zero-based: 1 -> MIDI channel 2.
    synth.play_note(member_channel, note, 100, 0)
process_one_block(synth)

# Queue a deliberately abusive same-sample face-roll. Every valid note-on must
# start even though CPU and memory remain bounded by N + 2 voices.
for index in range(128):
    status = 0x91 + (index % 15)  # MIDI channels 2..16.
    synth.queue_midi(bytes((status, 36 + (index % 84), 100)), 0)

audio = process_one_block(synth)
telemetry = synth.get_voice_telemetry()

assert telemetry["physical_pool_size"] == 18
assert telemetry["configured_voice_limit"] == VOICE_LIMIT
assert telemetry["musical_voices"] <= VOICE_LIMIT
assert telemetry["fast_fade_voices"] <= 2
assert telemetry["rendered_voices"] <= VOICE_LIMIT + 2
assert telemetry["note_starts"] == 132
assert telemetry["dropped_note_ons"] == 0
assert np.all(np.isfinite(audio))

print("Fixed-pool voice containment passed:")
for key, value in telemetry.items():
    print(f"  {key}: {value}")
