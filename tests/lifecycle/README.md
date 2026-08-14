# Voice lifecycle harness

This harness checks bounded voice-manager behavior directly. Raw MIDI messages
use their actual 1-based MIDI channel; note
scenarios use lower-zone MPE member channels 2-16, while zone-wide sustain and
panic messages use master channel 1.

After building `Resonarium_Python` and `Resonarium_Python_Test`:

```sh
python3 tests/lifecycle/harness.py list
python3 tests/lifecycle/harness.py check --module build/python_modules/resonarium.so --suite core
python3 tests/lifecycle/harness.py stress --module build/python_modules/resonarium.so --events 100000
python3 tests/lifecycle/audio_invariants.py --module build/python_modules/resonarium_test.so
```

Every processed block asserts an 18-voice physical pool, musical voices no higher
than `numVoices`, at most two fast fades, no more than `numVoices + 2` rendered
voices, monotonic counters, and zero dropped note-ons. The harness writes reports
only to its requested output path.

`audio_invariants.py` renders the native processor and checks the production
signal path without fixtures. It covers gain-before-mix additivity, private-tail
retirement, the single-global/zero-voice effect-chain topology, the legacy
`polyEffectChain` audio tombstone, global tails surviving voice retirement,
delay disable/re-enable history reset, sample-accurate CC120 clearing of the
global delay tail, and global-effect work remaining independent of voice count.
