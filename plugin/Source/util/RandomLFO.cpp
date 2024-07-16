//
// Created by Gabriel Soule on 7/10/24.
//

#include "RandomLFO.h"

RandomLFO::RandomLFO(gin::ModVoice* voice, bool stereo) : voice(voice), stereo(stereo)
{
}

RandomLFO::RandomLFO(RandomLFOParams params, bool stereo) : params(params), stereo(stereo)
{
}

RandomLFO::RandomLFO(gin::ModVoice* voice, RandomLFOParams params, bool stereo) : params(params), stereo(stereo)
{
}


void RandomLFO::prepare(const juce::dsp::ProcessSpec& spec)
{
    this->sampleRate = spec.sampleRate;
    this->oneOverSampleRate = 1.0f / sampleRate;
    centerState.rng.setSeed(params.seed);
    leftState.rng.setSeed(centerState.rng.nextInt());
    rightState.rng.setSeed(leftState.rng.nextInt());
    centerState.lastRandomValue = centerState.rng.nextFloat();
    centerState.nextRandomValue = centerState.rng.nextFloat();
    leftState.lastRandomValue = leftState.rng.nextFloat();
    leftState.nextRandomValue = leftState.rng.nextFloat();
    rightState.lastRandomValue = rightState.rng.nextFloat();
    rightState.nextRandomValue = rightState.rng.nextFloat();
}

void RandomLFO::updateParameters(float frequency)
{
    this->rate = frequency;
    this->sync = voice->getValue(params.sync);
    this->mode = static_cast<Mode>(static_cast<int>(voice->getValue(params.mode)));
    this->depth = voice->getValue(params.depth);
    this->offset = voice->getValue(params.offset);
    this->smooth = voice->getValue(params.smooth);
    this->chaos = voice->getValue(params.chaos);
    this->jitter = voice->getValue(params.jitter);
    this->stereoAmount = voice->getValue(params.stereo);
}

void RandomLFO::updateParametersMono(gin::ModMatrix& matrix, float frequency)
{
    this->rate = frequency;
    this->sync = matrix.getValue(params.sync);
    this->mode = static_cast<Mode>(static_cast<int>(matrix.getValue(params.mode)));
    this->depth = matrix.getValue(params.depth);
    this->offset = matrix.getValue(params.offset);
    this->smooth = matrix.getValue(params.smooth);
    this->chaos = matrix.getValue(params.chaos);
    this->jitter = matrix.getValue(params.jitter);
    this->stereoAmount = matrix.getValue(params.stereo);
}

void RandomLFO::reset()
{
}

void RandomLFO::noteOn(float phase)
{
}

void RandomLFO::processInternal(int numSamples, RandomState& state)
{
    phaseDelta = rate * oneOverSampleRate * numSamples;
    currentPhase += phaseDelta;
    if (currentPhase >= 1.0f)
    {
        currentPhase -= 1.0f;
        float min = std::max(0.0f, state.lastRandomValue - state.lastRandomValue * chaos);
        float max = std::min(1.0f, state.lastRandomValue + (1.0f - state.lastRandomValue) * chaos);

        static constexpr uint32_t MAX_UINT = std::numeric_limits<uint32_t>::max();
        state.lastRandomValue = state.nextRandomValue;

        // Convert signed int to unsigned int before casting to float
        uint32_t unsignedRandom = static_cast<uint32_t>(state.rng.nextInt()) + 0x80000000;
        state.nextRandomValue = min + (static_cast<float>(unsignedRandom) / static_cast<float>(MAX_UINT)) * (max - min);
    }

    jassert(state.lastRandomValue >= 0.0f && state.lastRandomValue <= 1.0f);
    jassert(state.nextRandomValue >= 0.0f && state.nextRandomValue <= 1.0f);

    state.currentRandomValue = perlinInterpolate(state.lastRandomValue, state.nextRandomValue, currentPhase);
}

float RandomLFO::process(int numSamples)
{
    processInternal(numSamples, centerState);
}

float RandomLFO::getOutput()
{
    return (centerState.currentRandomValue * 2.0) - 1.0f;
}

float RandomLFO::getOutputUnclamped()
{
}

float RandomLFO::getOutput(int channel)
{
}

float RandomLFO::getOutputUnclamped(int channel)
{
}

// float RandomLFO::perlinInterpolate(float a, float b, float t)
// {
// const float interpolate_from = a * t;
// const float interpolate_to = b * (t - 1.0f);
// const float interpolate_t = t * t * (t * -2.0f + 3.0f);
// return (interpolate_from + (interpolate_to - interpolate_from) * interpolate_t) * 2.0f;
// }

float RandomLFO::perlinInterpolate(float from, float to, float t)
{
    // Smoothstep function
    t = t * t * (3.0f - 2.0f * t);

    // Linear interpolation using the smoothstepped t
    return (from * (1.0f - t) + to * t);
}