import resonarium
import numpy as np
from scipy.io import wavfile

def write_to_wav(buffer, filename):
    audio_data = buffer.T

    # Normalize the audio to prevent clipping
    max_val = np.max(np.abs(audio_data))
    if max_val > 0:
        audio_data = audio_data / max_val

    # Convert to 16-bit PCM format
    audio_data_16bit = (audio_data * 32767).astype(np.int16)

    # Save as WAV file
    wavfile.write(filename, int(sample_rate), audio_data_16bit)

# Initialize the synth with default settings (44.1kHz, 512 block size)
synth = resonarium.Resonarium()

# Let's see what parameters we can play with...
# for param in synth.get_all_params():
#     print(f"{param.id}: {param.value}")
#     print(f"     Range: {param.min} to {param.max}")

# Everything is disabled by default. Let's enable the impulse exciter and a single string model, with a bright loop filter
# "wb0r0" corresponds to the first string model in the first waveguide bank -- indexing starts at zero. There are four waveguide banks, with eight string models apiece.
synth.set_param("impExciter0 enabled", 1.0)
synth.set_param("enabled wb0r0", 1.0)
synth.set_param("decayFilterCutoff wb0r0", 7000)

# Create an audio buffer that stores a few seconds of audio
sample_rate = synth.get_sample_rate()
block_size = synth.get_block_size()
seconds = 2.0

# Calculate how many blocks we need for the number of seconds
blocks_needed = int(np.ceil((sample_rate * seconds) / block_size))

# Allocate the corresponding memory
audio_buffer = synth.create_multi_block(blocks_needed)

# Play a MIDI note (middle C = 60) with velocity 100
synth.play_note(0, 60, 100)

# Process audio. This will fill the buffer with audio, and stop when the buffer is full.
synth.process_multi_block(audio_buffer)

# We might want to add MIDI events or change parameters during processing.
# In that case, we can call process_multi_block multiple times and specify the start and end blocks.
# We'll have to increment the current block index on the Python side, since the buffer doesn't "remember" the current write position.
# e.g.: synth.process_multi_block(audio_buffer, start_block, end_block)

write_to_wav(audio_buffer, "processor_example.wav")

# In this example we directly manipulate and read audio from a single voice
audio_buffer.fill(0.0)
synth = resonarium.Resonarium()
synth.set_param("impExciter0 enabled", 1.0)
synth.set_param("enabled wb0r0", 1.0)
synth.set_param("decayFilterCutoff wb0r0", 7000)

# Get the 0th voice in the voices array. The voices array, in theory, should not be modified after instantiation...
# therefore, this should always return a pointer to the same voice object in memory (if you always use the same index)
voice = synth.get_voice(0)

# Make the voice play a note
voice.play_note(60, 100) # notice
voice.process_multi_block(audio_buffer)
voice.release_note()
voice.reset()

# You can now have the voice generate more data into a new buffer by repeating the above four lines
# just make sure to start, stop, and reset the voice, and make sure the buffer is cleared each time

write_to_wav(audio_buffer, "voice_example.wav")
print("Done! :)")
