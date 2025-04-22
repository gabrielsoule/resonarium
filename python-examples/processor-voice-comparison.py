#!/usr/bin/env python3
# This script shows how audio can be extracted from a single voice instead of from the raw audio processor output.
# This is more efficient for generating large datasets.

import resonarium
import numpy as np
from scipy.io import wavfile
import time

# Configuration for both examples
SAMPLE_RATE = 44100
BLOCK_SIZE = 512
NOTE_NUMBER = 60
VELOCITY = 100
DURATION_SECONDS = 2.0

print("Resonarium Voice vs Processor Comparison Demo")
print("=============================================")

# Function to save audio files
def save_wav(buffer, filename):
    # Convert from (channels, samples) to (samples, channels)
    audio_data = buffer.T
    # No normalization - save raw values
    # Clip to avoid out-of-range values and convert to 16-bit PCM
    audio_data_16bit = np.clip(audio_data * 32767, -32768, 32767).astype(np.int16)
    # Save
    wavfile.write(filename, SAMPLE_RATE, audio_data_16bit)

def compare(buf1, buf2):
    # Calculate the difference between the two buffers
    difference = np.abs(buf1 - buf2)
    max_difference = np.max(difference)
    mean_difference = np.mean(difference)

    # Determine if the outputs match (with some tolerance for floating point precision)
    tolerance = 1e-5
    if max_difference < tolerance:
        print(f"  ✓ The outputs match within tolerance: max difference = {max_difference:.6f} < {tolerance}")
    else:
        print("  ✗ The outputs differ significantly. This should never happen!")
        # Check buffer shapes
        print(f"  Processor buffer shape: {buf1.shape}")
        print(f"  Voice buffer shape: {buf2.shape}")
        # Check buffer contents
        print(f"  Processor buffer min/max: {np.min(buf1):.6f}/{np.max(buf1):.6f}")
        print(f"  Voice buffer min/max: {np.min(buf2):.6f}/{np.max(buf2):.6f}")
        # Sample of difference values
        print(f"  Difference pattern sample (first 5 out of {difference.size} values):")
        flat_diff = difference.flatten()
        for j in range(min(5, flat_diff.size)):
            if flat_diff[j] > 0:
                print(f"    Index {j}: {flat_diff[j]:.10f}")


# ==============================================================================
# Part 1: Basic comparison of processor vs voice approach
# ==============================================================================
print("\n[1] Basic Single Comparison Test")
print("-----------------------------")
print("Running basic comparison between processor and voice approaches...")

# Initialize the synth
synth_processor = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)

# Configure the synth
synth_processor.set_param("impExciter0 enabled", 1.0)
synth_processor.set_param("enabled wb0r0", 1.0)
synth_processor.set_param("decayFilterCutoff wb0r0", 7000)

# Calculate how many blocks we need for the duration
blocks_needed = int(np.ceil((SAMPLE_RATE * DURATION_SECONDS) / BLOCK_SIZE))

# Allocate the audio buffer
processor_buffer = synth_processor.create_multi_block(blocks_needed)

# Play a note and process audio
# NOTE_NUMBER = np.random.randint(60, 84)
synth_processor.play_note(0, NOTE_NUMBER, VELOCITY)
synth_processor.process_multi_block(processor_buffer)

print(f"  - Generated {DURATION_SECONDS} seconds of audio using processor")

# Initialize a new synth, same as above
synth_voice = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)

# Configure the synth the same way
synth_voice.set_param("impExciter0 enabled", 1.0)
synth_voice.set_param("enabled wb0r0", 1.0)
synth_voice.set_param("decayFilterCutoff wb0r0", 7000)

# Acquire a voice directly
voice = synth_voice.get_voice(0)  # Get the first voice

# Create a buffer using the same method as with the processor
voice_buffer = synth_voice.create_multi_block(blocks_needed)

# Play a note directly on the voice
voice.play_note(NOTE_NUMBER, VELOCITY)

# Process audio using the new process_multi_block method
voice.process_multi_block(voice_buffer)

voice.release_note()
voice.reset()

print(f"  - Generated {DURATION_SECONDS} seconds of audio using voice wrapper")

compare(processor_buffer, voice_buffer)

# ==============================================================================
# Part 2: Generate multiple notes from the voice and compare them to the baseline processor note
# ==============================================================================
print("\n[2] Voice Reuse Comparison Test")
print("----------------------------------------------")
ITERATIONS_COMPARISON_TEST = 10
print(f"Running {ITERATIONS_COMPARISON_TEST} iterations of voice comparison...")

total_time = 0
for i in range(ITERATIONS_COMPARISON_TEST):
    print(f"\nIteration {i + 1}/{ITERATIONS_COMPARISON_TEST}")
    start_time = time.time()

    voice_buffer_2 = synth_voice.create_multi_block(blocks_needed)
    voice.reset()
    voice.play_note(NOTE_NUMBER, VELOCITY)
    voice.process_multi_block(voice_buffer_2)
    voice.release_note()

    iteration_time = time.time() - start_time
    total_time += iteration_time
    print(f"Time taken: {iteration_time:.3f} seconds")

    compare(processor_buffer, voice_buffer_2)

print(f"\nAverage processing time per iteration: {total_time / ITERATIONS_COMPARISON_TEST:.3f} seconds")

print("\n[3] Voice Reuse Performance Test with Varying Notes")
print("----------------------------------------------")
ITERATIONS_PERFORMANCE_TEST = 3  # Reduced to save disk space
print(f"Running {ITERATIONS_PERFORMANCE_TEST} iterations with different notes...")

DECAY_CUTOFF = 5000  # Fixed value for consistent comparison


# Create a reusable buffer for the voice approach
reusable_synth = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)
reusable_synth.set_param("impExciter0 enabled", 1.0)
reusable_synth.set_param("enabled wb0r0", 1.0)
reusable_synth.set_param("decayFilterCutoff wb0r0", DECAY_CUTOFF)
reusable_voice = reusable_synth.get_voice(0)
reusable_buffer = reusable_synth.create_multi_block(blocks_needed)

total_time_processor = 0
total_time_voice = 0


for i in range(ITERATIONS_PERFORMANCE_TEST):
    # Use a different note for each iteration to test for discrepancies
    current_note = NOTE_NUMBER
    
    # New processor approach (baseline)
    start_time = time.time()

    # Use a different decay cutoff for each test to see if it affects matching

    # Create a new processor
    new_processor = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)
    new_processor.set_param("impExciter0 enabled", 1.0)
    new_processor.set_param("enabled wb0r0", 1.0)
    new_processor.set_param("decayFilterCutoff wb0r0", DECAY_CUTOFF)
    
    # Create a new buffer
    processor_buffer_new = new_processor.create_multi_block(blocks_needed)
    processor_buffer_new.fill(0.0)

    # Play a note and process audio
    new_processor.play_note(0, current_note, VELOCITY)
    new_processor.process_multi_block(processor_buffer_new)
    
    processor_time = time.time() - start_time
    total_time_processor += processor_time
    
    # Voice reuse approach with reused buffer
    start_time = time.time()
    
    # Clear the buffer by filling it with zeros
    reusable_buffer.fill(0.0)

    print(reusable_synth.get_param("decayFilterCutoff wb0r0"))
    print(new_processor.get_param("decayFilterCutoff wb0r0"))

    # Reuse the voice
    voice.reset()
    voice.play_note(current_note, VELOCITY)
    voice.process_multi_block(reusable_buffer)
    voice.release_note()
    
    voice_time = time.time() - start_time
    total_time_voice += voice_time
    
    # Save both audio files to compare
    save_wav(processor_buffer_new, f'processor_note_{current_note}.wav')
    save_wav(reusable_buffer, f'voice_note_{current_note}.wav')
    
    # Compare the outputs to verify they match
    difference = np.abs(processor_buffer_new - reusable_buffer)
    max_difference = np.max(difference)
    
    tolerance = 1e-5
    
    # If they differ, save an amplified version of the difference and detailed info
    if max_difference > tolerance:
        # Create an amplified version of the difference for auditory comparison
        diff_buffer = difference * 1000.0  # Amplify the difference
        save_wav(diff_buffer, f'difference_note_{current_note}.wav')
        
        # Find where the first significant difference occurs
        flat_diff = difference.flatten()
        first_diff_idx = np.argmax(flat_diff > tolerance)
        sample_idx = first_diff_idx % processor_buffer_new.shape[1]
        channel_idx = first_diff_idx // processor_buffer_new.shape[1]
        
        print(f"  ✗ Outputs differ for note {current_note}: max difference {max_difference:.6f}")
        
        if first_diff_idx < flat_diff.size:
            print(f"  First significant difference at channel {channel_idx}, sample {sample_idx}")
            print(f"  Processor value: {processor_buffer_new[channel_idx, sample_idx]}")
            print(f"  Voice value: {reusable_buffer[channel_idx, sample_idx]}")
            print(f"  Difference: {flat_diff[first_diff_idx]}")
        
        # Check if difference is consistent
        std_diff = np.std(difference[difference > tolerance])
        print(f"  Standard deviation of differences: {std_diff:.8f}")
        if std_diff < 0.00001:
            print(f"  The differences appear very consistent (low standard deviation)")
            
        # Check if the differences scale with note number
        print(f"  Note MIDI number: {current_note}, Note frequency: {440 * 2**((current_note-69)/12):.2f} Hz")
    else:
        print(f"Iteration {i+1}: Note {current_note} - Outputs match within tolerance")

avg_time_processor = total_time_processor / ITERATIONS_PERFORMANCE_TEST
avg_time_voice = total_time_voice / ITERATIONS_PERFORMANCE_TEST
speedup = avg_time_processor / avg_time_voice

print(f"\nPerformance Results:")
print(f"Average time with new processor:  {avg_time_processor:.6f} seconds")
print(f"Average time with reused voice:   {avg_time_voice:.6f} seconds")
print(f"Voice reuse is {speedup:.2f}x faster than creating new processors")
print("\nAudio files have been saved for each note for detailed comparison.")


save_wav(processor_buffer, 'processor_output.wav')
save_wav(voice_buffer, 'voice_output.wav')

# ==============================================================================
# Part 4: Test determinism across multiple synth instances
# ==============================================================================
print("\n[4] Multiple Synth Determinism Test")
print("--------------------------------")

NUM_SYNTHS = 5  # Number of synth instances to create
NUM_ITERATIONS = 30  # Number of test iterations
RESET_BETWEEN = True  # Whether to reset voices between iterations

print(f"Testing determinism across {NUM_SYNTHS} synth instances over {NUM_ITERATIONS} iterations...")

# Create multiple synth instances
synths = []
voices = []
buffers = []

# Initialize all synths with identical parameters
for i in range(NUM_SYNTHS):
    print(f"Creating synth {i+1}/{NUM_SYNTHS}...")
    synth = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)
    synth.set_param("impExciter0 enabled", 1.0)
    synth.set_param("enabled wb0r0", 1.0)

    # Get a voice from each synth
    voice = synth.get_voice(0)
    
    # Create a buffer for each synth
    buffer = synth.create_multi_block(blocks_needed)
    
    synths.append(synth)
    voices.append(voice)
    buffers.append(buffer)

# Run the test iterations
for iteration in range(NUM_ITERATIONS):
    print(f"\nIteration {iteration+1}/{NUM_ITERATIONS}")
    
    # Select a note for this iteration (could vary between iterations)
    current_note = NOTE_NUMBER + iteration % 12
    
    # For each synth/voice pair
    for i in range(NUM_SYNTHS):
        synths[i].set_param("decayFilterCutoff wb0r0", DECAY_CUTOFF + (iteration * 85) % 1000)
        # Clear the buffer
        buffers[i].fill(0.0)
        
        # Reset the voice if requested
        if RESET_BETWEEN or iteration == 0:
            voices[i].reset()
        
        # Play the same note on all voices
        voices[i].play_note(current_note + iteration % 10, VELOCITY)
        
        # Process the audio
        voices[i].process_multi_block(buffers[i])
        
        # Release the note
        voices[i].release_note()
    
    # Compare all buffers to the first one
    reference_buffer = buffers[0]
    
    # Save the reference buffer for this iteration
    if iteration < 3:  # Save only first few to avoid filling disk
        save_wav(reference_buffer, f'reference_iter_{iteration+1}_note_{current_note}.wav')
    
    # Compare each buffer with the reference buffer
    all_match = True
    for i in range(1, NUM_SYNTHS):
        difference = np.abs(reference_buffer - buffers[i])
        max_difference = np.max(difference)
        tolerance = 1e-5
        
        if max_difference > tolerance:
            all_match = False
            print(f"  ✗ Synth {i+1} differs from reference: max difference = {max_difference:.8f}")
            
            # Save the differing buffer and the difference for the first few iterations
            if iteration < 3:
                save_wav(buffers[i], f'synth{i+1}_iter_{iteration+1}_note_{current_note}.wav')
                diff_buffer = difference * 1000.0  # Amplify for audibility
                save_wav(diff_buffer, f'diff_synth{i+1}_iter_{iteration+1}_note_{current_note}.wav')
            
            # Analyze where the difference occurs
            flat_diff = difference.flatten()
            first_diff_idx = np.argmax(flat_diff > tolerance)
            if first_diff_idx < flat_diff.size:
                sample_idx = first_diff_idx % reference_buffer.shape[1]
                channel_idx = first_diff_idx // reference_buffer.shape[1]
                print(f"    First significant difference at channel {channel_idx}, sample {sample_idx}")
                print(f"    Reference value: {reference_buffer[channel_idx, sample_idx]}")
                print(f"    Synth {i+1} value: {buffers[i][channel_idx, sample_idx]}")
    
    if all_match:
        print(f"  ✓ All synths produced identical output for note {current_note}")
        print(f"    Example sample: {reference_buffer[0, 10000]}")
    else:
        print(f"  ✗ Outputs differ across synths for note {current_note}")

# ==============================================================================
# Part 5: Test determinism across consecutive runs with the same synth/voice
# ==============================================================================
print("\n[5] Consecutive Runs Determinism Test")
print("----------------------------------")

NUM_CONSECUTIVE_RUNS = 20  # Number of consecutive runs to test
RESET_BETWEEN_RUNS = True  # Whether to reset the voice between runs

print(f"Testing determinism across {NUM_CONSECUTIVE_RUNS} consecutive runs with the same synth...")

# Create a single synth instance for this test
test_synth = resonarium.Resonarium(SAMPLE_RATE, BLOCK_SIZE)
test_synth.set_param("impExciter0 enabled", 1.0)
test_synth.set_param("enabled wb0r0", 1.0)
test_synth.set_param("decayFilterCutoff wb0r0", 5000)

# Get a voice
test_voice = test_synth.get_voice(0)

# Create buffers for comparison
reference_buffer = test_synth.create_multi_block(blocks_needed)
test_buffer = test_synth.create_multi_block(blocks_needed)

# Test with different notes
for note_offset in range(min(5, NUM_CONSECUTIVE_RUNS)):
    current_note = NOTE_NUMBER + note_offset
    print(f"\nTesting note: {current_note}")
    
    # Generate reference output
    test_synth.set_param("decayFilterCutoff wb0r0", DECAY_CUTOFF + (note_offset * 85) % 1000)
    reference_buffer.fill(0.0)
    test_voice.reset()
    test_voice.play_note(current_note, VELOCITY)
    test_voice.process_multi_block(reference_buffer)
    test_voice.release_note()
    
    # Save reference for the first few notes
    if note_offset < 3:
        save_wav(reference_buffer, f'consecutive_ref_note_{current_note}.wav')
    
    # Run multiple times and compare
    for run in range(NUM_CONSECUTIVE_RUNS):
        # Clear the buffer
        test_buffer.fill(0.0)
        
        # Reset the voice if requested
        if RESET_BETWEEN_RUNS:
            test_voice.reset()
        
        # Play the note
        test_voice.play_note(current_note, VELOCITY)
        test_voice.process_multi_block(test_buffer)
        test_voice.release_note()
        
        # Compare with reference
        difference = np.abs(reference_buffer - test_buffer)
        max_difference = np.max(difference)
        tolerance = 1e-5
        
        if max_difference > tolerance:
            print(f"  ✗ Run {run+1} differs from reference: max difference = {max_difference:.8f}")
            
            # Save differing output for the first few runs
            if run < 3 and note_offset < 3:
                save_wav(test_buffer, f'consecutive_run{run+1}_note_{current_note}.wav')
                diff_buffer = difference * 1000.0  # Amplify
                save_wav(diff_buffer, f'consecutive_diff{run+1}_note_{current_note}.wav')
            
            # Analyze the difference
            flat_diff = difference.flatten()
            first_diff_idx = np.argmax(flat_diff > tolerance)
            if first_diff_idx < flat_diff.size:
                sample_idx = first_diff_idx % reference_buffer.shape[1]
                channel_idx = first_diff_idx // reference_buffer.shape[1]
                print(f"    First significant difference at channel {channel_idx}, sample {sample_idx}")
                print(f"    Reference value: {reference_buffer[channel_idx, sample_idx]}")
                print(f"    Run {run+1} value: {test_buffer[channel_idx, sample_idx]}")
                print(f"    Difference: {flat_diff[first_diff_idx]}")
        else:
            print(f"  ✓ Run {run+1} matches reference")
    
    print(f"Completed {NUM_CONSECUTIVE_RUNS} consecutive runs for note {current_note}")

print("\nDone!")