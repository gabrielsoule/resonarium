# Python Bindings

Resonarium supports basic Python bindings. They are a work in progress. At the moment, the bindings support getting/setting synth parameters, playing MIDI notes, modulating parameters, and recording audio to memory.

To compile the Python library, build the `Resonarium_Python` CMake target, e.g.:

```bash
cmake --build build --target Resonarium_Python
```

You'll find the `.so` library file in the build directory. By default, the library is not installed system-wide. Only MacOS is supported as of writing.

The bindings can be found in `ResonariumPy.cpp`. An LLM should be able to generate documentation given the `.cpp` file and the appropriate prompt. The simple script below demonstrates how to manipulate parameters, play MIDI, and record the result.

```python
import resonarium
import numpy as np
from scipy.io import wavfile  

# Initialize the synth with default settings (44.1kHz, 512 block size)
synth = resonarium.Resonarium()

# Let's see what parameters we can play with...
for param in synth.get_all_params():
    print(param.id + " : " + param.value)
    print("     " + param.min + " to " + param.max)

# Everything is disabled by default. Let's enable the impulse exciter and a single string model, with a bright loop filter
# "wb0r0" corresponds to the first string model in the first waveguide bank -- indexing starts at zero. There are four waveguide banks, with eight string models apiece.
synth.set_param("impExciter0 enabled", 1.0)
synth.set_param("enabled wb0r0", 1.0)
synth.set_param("decayFilterCutoff wb0r0", 7000)

# Create an audio buffer that stores about 4 seconds of audio
sample_rate = synth.get_sample_rate()
block_size = synth.get_block_size()
seconds = 4.0

# Calculate how many blocks we need for 4 seconds
blocks_needed = int(np.ceil((sample_rate * seconds) / block_size))

# Allocate the corresponding memory
audio_buffer = synth.create_multi_block(blocks_needed)

# Play a MIDI note (middle C = 60) with velocity 100
synth.play_note(0, 60, 100)

# Process audio. This will fill the buffer with audio, and stop when the buffer is full.
# We allocated about 4 seconds worth of audio blocks, so we should get a 4 second sample back.
synth.process_multi_block(audio_buffer)

# We might want to add MIDI events or change parameters during processing. 
# In that case, we can call process_multi_block multiple times and specify the start and end blocks.
# We'll have to increment the current block index on the Python side, since the buffer doesn't "remember" the current write position.
# e.g.: synth.process_multi_block(audio_buffer, start_block, end_block)

# Convert the numpy array to the right format for saving
# The audio_buffer is in (channels, samples) format, we need (samples, channels)
audio_data = audio_buffer.T

# Normalize the audio to prevent clipping
max_val = np.max(np.abs(audio_data))
if max_val > 0:
    audio_data = audio_data / max_val

# Convert to 16-bit PCM format
audio_data_16bit = (audio_data * 32767).astype(np.int16)

# Save as WAV file
wavfile.write('resonarium_output.wav', int(sample_rate), audio_data_16bit)
print("All done! :)")
```

## Parameter IDs
At the moment, there are no higher-level Python structures (e.g. LFOs, MSEGs, etc) that map directly to their internal C++ counterparts. Instead, Resonarium's internal state is manipulated through direct access to internal and external parameters. The parameter ID naming scheme is somewhat inconsistent; this will be changed eventually. 

All the parameters are defined in `Parameters.cpp`. You can review this file manually to see what parameters are available, or feed it into an LLM and ask questions.
