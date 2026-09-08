# Resonarium tests

## JUCE source integrity

JUCE is an immutable upstream dependency in this repository. Every CMake
configure, packaging job, and publishing job must run the
same fail-closed check before compiling anything:

```sh
python3 tests/ci/verify_pristine_juce.py --output juce-integrity.json
```

The check requires `modules/juce` to be a stage-0 `160000` gitlink, requires
the index and checked-out JUCE `HEAD` to equal the gitlink recorded by the
superproject `HEAD`, and rejects tracked, staged, nested-submodule, and
untracked changes inside JUCE. It writes `resonarium-pristine-juce-v1` JSON
evidence on both success and failure.

Build artifacts created while JUCE differed from the committed gitlink must be
rebuilt from a passing checkout.

The native unit tests use Catch2 3.7.1 and follow the same layout and conventions
as Applause: test directories mirror production source directories, test files end
in `Test.cpp`, and CTest discovers individual Catch2 cases.

Native tests are opt-in so normal plugin builds do not fetch or build Catch2:

```sh
cmake -B build-tests -DCMAKE_BUILD_TYPE=Debug -DRESONARIUM_BUILD_TESTS=ON
cmake --build build-tests --target resonarium_tests
ctest --test-dir build-tests -L native --output-on-failure
```

The tests currently compile only the production DSP sources they exercise. As the
audio engine is separated from the plugin shell, the test executable can link the
resulting engine library directly, as Applause does.

The MIDI compatibility check renders the production Python processor with raw
MIDI messages on every channel, including channel 1 (GitHub issue #39):

```sh
cmake --build build --target Resonarium_MidiCompatibility
ctest --test-dir build -R '^Resonarium\.MidiCompatibility$' --output-on-failure
```

The production exciter rack has two deterministic Python checks. The audio
contract covers its PRNG streams, sample snapshots, and shared external input;
the rack contract covers source masks, control caching, additivity, MIDI
boundaries, and assignment lifecycles:

```sh
cmake --build build --config Release --target Resonarium_ExciterAudioContract
ctest --test-dir build -C Release \
  -R '^Resonarium\.Exciter(Audio|Rack)Contract$' \
  --output-on-failure
```

Factory presets have a separate production-module sweep. Each preset is loaded
through Gin's bundled-program path, its serialized modulation/MSEG/sample state
is checked after round-trip, and a short render is checked for finite output:

```sh
cmake --build build --config Release --target Resonarium_PresetSweep
```

The bounded voice manager has a separate deterministic harness. It sends raw MPE
messages, samples public voice telemetry after every block, and includes a
100,000-event deterministic stress stream:

```sh
cmake --build build-tests --target Resonarium_VoiceLifecycle
cmake --build build-tests --target Resonarium_VoiceStress
```

The audio-invariant gates must use a Release build. They exercise the
real native processor, an audible 15-voice/eight-resonator workload, private
voice mixing and retirement, and the single retained global-effect-chain
topology, tails, reset behavior, compatibility tombstone, and bounded work:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DRESONARIUM_BUILD_TESTS=ON
cmake --build build --target Resonarium_AudioInvariants
cmake --build build --target Resonarium_VoicePerformance
ctest --test-dir build -C Release \
  -R '^Resonarium\.(AudioInvariants|VoicePerformance)$' \
  --output-on-failure
```

On macOS, `resonarium_rt_allocation_gate` interposes both C and C++ allocation
entry points around `processBlock`. It also enforces the prepared/warmed RSS
budgets and covers active resonators, every retained global-effect enable and
parameter-update path, legacy poly-FX tombstone automation, stealing, sustain,
panic, voice-limit, and 100,000-event phases:

```sh
cmake --build build --target resonarium_rt_allocation_gate
ctest --test-dir build -C Release \
  -R '^Resonarium\.RTAllocationGate$' \
  --output-on-failure
```
